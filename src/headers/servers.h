#ifndef HEADERS_SERVERS_H
#define HEADERS_SERVERS_H

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <uv.h>

#include "error.h"
#include "state_machine.h"

#define UNUSED(param) (void) (param);
#define N_BACKLOG 64
#define MAXFDS 16 * 1024
#define SEND_BUF_SIZE 1024
typedef struct {
    int sockfd;
} thread_config_t;

typedef struct {
    bool want_read;
    bool want_write;
} fd_status_t;

typedef struct {
    ProcessingState state;
    uint8_t send_buf[SEND_BUF_SIZE];
    int send_buf_end;
    int send_ptr;
    // NOTE: libuv usage
    uv_tcp_t* client;
} peer_state_t;

static struct {
    fd_status_t READ;
    fd_status_t WRITE;
    fd_status_t READ_WRITE;
    fd_status_t NO_READ_WRITE;
} fd_status_mode_t = {
    .READ = {.want_read = true, .want_write = false},
    .WRITE = {.want_read = false, .want_write = true},
    .READ_WRITE = {.want_read = true, .want_write = true},
    .NO_READ_WRITE = {.want_read = false, .want_write = false},
};

// each peer is globally identified by the file descriptor (fd) it's connected
// on. As long as the peer is connected, the fd is unique to it. When a peer
// disconnects, a new peer may connect and get the same fd. on_peer_connected
// should initialize the state properly to remove any trace of the old peer on
// the same fd
static peer_state_t global_state[MAXFDS];

void sequential_server(int sockfd);
void thread_server(int sockfd);
void event_driven_select_server(int sockfd);
void event_driven_epoll_server(int sockfd);
int event_driven_libuv_server(int port);

void blocking_sock_connection(int sockfd);
void nonblocking_sock_connection(int sockfd);

int
listen_inet_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1) {
        errlog("error to start the INET/TCP socket connection");
    }

    int opt = 1;

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        errlog("error to set socket options");
    }

    struct sockaddr_in serv_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) {
        errlog("error to bind socket");
    }

    if (listen(sockfd, N_BACKLOG) == -1) {
        errlog("error on listen socket");
    }

    return sockfd;
}

void
log_peer_connection(const struct sockaddr_in* sa, socklen_t salen) {
    char hostbuf[NI_MAXHOST];
    char portbuf[NI_MAXSERV];

    if (getnameinfo((struct sockaddr*) sa, salen, hostbuf, NI_MAXHOST, portbuf, NI_MAXSERV, 0) == 0) {
        printf("peer '%s:%s' connected\n", hostbuf, portbuf);
    } else {
        printf("peer 'unknown' connected\n");
    }
}

void*
start_thread(void* arg) {
    thread_config_t* config = (thread_config_t*) arg;

    int sockfd = config->sockfd;

    free(config);

    unsigned long id = (unsigned long) pthread_self();

    printf("thread %lu created to handle connection with socket %d\n", id, sockfd);

    start_state_machine(sockfd);

    printf("thread %lu done\n", id);

    return 0;
}

void
make_sock_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);

    if (flags == -1) {
        errlog("error on fcntl to get flags");
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        errlog("error to set nonblocking mode on socket connection");
    }
}

fd_status_t
on_peer_connected(int sockfd, const struct sockaddr_in* peer_addr, socklen_t peer_addr_len) {
    assert(sockfd < MAXFDS);

    log_peer_connection(peer_addr, peer_addr_len);

    // NOTE: initialize state to send back a '*' to the peer immediately
    peer_state_t* peer_state = &global_state[sockfd];

    peer_state->state = INITIAL_ACK;
    peer_state->send_buf[0] = '*';
    peer_state->send_ptr = 0;
    peer_state->send_buf_end = 1;

    return fd_status_mode_t.WRITE;
}

fd_status_t
on_peer_ready_recv(int sockfd) {
    assert(sockfd < MAXFDS);

    peer_state_t* peer_state = &global_state[sockfd];

    if (peer_state->state == INITIAL_ACK || peer_state->send_ptr < peer_state->send_buf_end) {
        return fd_status_mode_t.WRITE;
    }

    uint8_t buf[1024];
    int bytes_len = recv(sockfd, buf, sizeof(buf), 0);

    if (bytes_len == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return fd_status_mode_t.READ;
        } else {
            errlog("error to receive socket data");
        }
    } else if (bytes_len == 0) {
        return fd_status_mode_t.NO_READ_WRITE;
    }

    bool ready_to_send = false;

    for (int i = 0; i < bytes_len; ++i) {
        switch (peer_state->state) {
            case INITIAL_ACK:
                assert(0 && "can't reach here");
                break;
            case WAITTING:
                if (buf[i] == '^') {
                    peer_state->state = PROCESSING;
                }
                break;
            case PROCESSING:
                if (buf[i] == '$') {
                    peer_state->state = WAITTING;
                } else {
                    assert(peer_state->send_buf_end < SEND_BUF_SIZE);

                    peer_state->send_buf[peer_state->send_buf_end++] = buf[i] + 1;
                    ready_to_send = true;
                }
                break;
        }
    }

    return (fd_status_t) {.want_read = !ready_to_send, .want_write = ready_to_send};
}

fd_status_t
on_peer_ready_send(int sockfd) {
    assert(sockfd < MAXFDS);

    peer_state_t* peer_state = &global_state[sockfd];

    if (peer_state->send_ptr >= peer_state->send_buf_end) {
        return fd_status_mode_t.READ_WRITE;
    }

    int send_len = peer_state->send_buf_end - peer_state->send_ptr;
    int sent_len = send(sockfd, &peer_state->send_buf[peer_state->send_ptr], send_len, 0);

    if (sent_len == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return fd_status_mode_t.WRITE;
        } else {
            errlog("error to send data on socket");
        }
    }

    if (sent_len < send_len) {
        peer_state->send_ptr += sent_len;

        return fd_status_mode_t.WRITE;
    } else {
        // NOTE: everything was sent successfully, reset the send queue
        peer_state->send_ptr = 0;
        peer_state->send_buf_end = 0;

        // NOTE: special-case state transition in if we were in INITIAL_ACK until now
        if (peer_state->state == INITIAL_ACK) {
            peer_state->state = WAITTING;
        }

        return fd_status_mode_t.READ;
    }
}

void
uv_on_client_closed(uv_handle_t* handle) {
    uv_tcp_t* client = (uv_tcp_t*) handle;

    if (client->data) {
        free(client->data);
    }

    free(client);
}

void
uv_on_wrote_buffer(uv_write_t* req, int status) {
    if (status) {
        errlog("libuv error to write on connection: %s", uv_strerror(status));
    }

    peer_state_t* peerstate = (peer_state_t*) req->data;

    // NOTE: if the message ends with 'WXY' finish the connection and close the main event loop
    if (peerstate->send_buf_end >= 3 && peerstate->send_buf[peerstate->send_buf_end - 3] == 'X'
        && peerstate->send_buf[peerstate->send_buf_end - 2] == 'Y'
        && peerstate->send_buf[peerstate->send_buf_end - 1] == 'Z') {
        free(peerstate);
        free(req);

        uv_stop(uv_default_loop());

        return;
    }

    peerstate->send_buf_end = 0;

    free(req);
}

void
uv_on_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    UNUSED(handle);

    buf->base = (char*) malloc(suggested_size);

    if (buf->base == NULL) {
        errlog("error to allocate memory");
    }

    buf->len = suggested_size;
}

void
uv_on_peer_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
    if (nread < 0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "libuv error reading connection: %s\n", uv_strerror(nread));
        }

        uv_close((uv_handle_t*) client, uv_on_client_closed);
    } else if (nread == 0) {
        // NOTE: don't do nothing is not an error
    } else {
        assert((ssize_t) buf->len >= nread);

        peer_state_t* peerstate = (peer_state_t*) client->data;

        if (peerstate->state == INITIAL_ACK) {
            free(buf->base);

            return;
        }

        for (int i = 0; i < nread; i++) {
            switch (peerstate->state) {
                case INITIAL_ACK:
                    assert(0 && "can't reach here");
                    break;
                case WAITTING:
                    if (buf->base[i] == '^') {
                        peerstate->state = PROCESSING;
                    }

                    break;
                case PROCESSING:
                    if (buf->base[i] == '$') {
                        peerstate->state = WAITTING;
                    } else {
                        assert(peerstate->send_buf_end < SEND_BUF_SIZE);

                        peerstate->send_buf[peerstate->send_buf_end++] = buf->base[i] + 1;
                    }

                    break;
            }
        }

        if (peerstate->send_buf_end > 0) {
            uv_buf_t write_buf = uv_buf_init((char*) peerstate->send_buf, peerstate->send_buf_end);
            uv_write_t* write_req = (uv_write_t*) malloc(sizeof(*write_req));

            if (write_req == NULL) {
                errlog("error to allocate memory");
            }

            write_req->data = peerstate;

            int rc;

            if ((rc = uv_write(write_req, (uv_stream_t*) client, &write_buf, 1, uv_on_wrote_buffer)) < 0) {
                errlog("libuv error to write: %s", uv_strerror(rc));
            }
        }
    }

    free(buf->base);
}

void
uv_on_wrote_init_ack(uv_write_t* req, int status) {
    if (status) {
        errlog("libuv write error: %s", uv_strerror(status));
    }

    peer_state_t* peerstate = (peer_state_t*) req->data;

    peerstate->state = WAITTING;
    peerstate->send_buf_end = 0;

    int rc;

    if ((rc = uv_read_start((uv_stream_t*) peerstate->client, uv_on_alloc_buffer, uv_on_peer_read)) < 0) {
        errlog("libuv error to read connection: %s", uv_strerror(rc));
    }

    free(req);
}

void
uv_on_peer_connected(uv_stream_t* server_stream, int status) {
    if (status < 0) {
        fprintf(stderr, "%s:%d: peer connection error: %s\n", __FILE__, __LINE__, uv_strerror(status));

        return;
    }

    uv_tcp_t* client = (uv_tcp_t*) malloc(sizeof(*client));

    if (client == NULL) {
        errlog("error to allocate memory");
    }

    int rc;

    if ((rc = uv_tcp_init(uv_default_loop(), client)) == -1) {
        errlog("libuv client connection failed: %s", uv_strerror(rc));
    }

    client->data = NULL;

    if (uv_accept(server_stream, (uv_stream_t*) client) == 0) {
        struct sockaddr_storage peername;
        int namelen = sizeof(peername);

        if ((rc = uv_tcp_getpeername(client, (struct sockaddr*) &peername, &namelen)) < 0) {
            errlog("identify peer name failed: %s", uv_strerror(rc));
        }

        // uv_report_peer_connected((const struct sockaddr_in*) &peername, namelen);

        peer_state_t* peerstate = (peer_state_t*) malloc(sizeof(*peerstate));

        if (peerstate == NULL) {
            errlog("error to allocate memory");
        }

        peerstate->state = INITIAL_ACK;
        peerstate->send_buf[0] = '*';
        peerstate->send_buf_end = 1;
        peerstate->client = client;

        client->data = peerstate;

        uv_buf_t write_buf = uv_buf_init((char*) peerstate->send_buf, peerstate->send_buf_end);
        uv_write_t* req = (uv_write_t*) malloc(sizeof(*req));

        if (req == NULL) {
            errlog("error to allocate memory");
        }

        req->data = peerstate;

        if ((rc = uv_write(req, (uv_stream_t*) client, &write_buf, 1, uv_on_wrote_init_ack)) < 0) {
            errlog("libuv write on connection error: %s", uv_strerror(rc));
        }
    } else {
        uv_close((uv_handle_t*) client, uv_on_client_closed);
    }
}

#endif
