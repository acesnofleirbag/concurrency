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

#include "state_machine.h"

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

void sequential_server(int);
void thread_server(int);
void event_driven_select_server(int);
void event_driven_epoll_server(int);

void blocking_sock_connection(int);
void nonblocking_sock_connection(int);

int
listen_inet_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1) {
        perror("servers.h:64: error to start the INET/TCP socket connection\n");
        exit(1);
    }

    int opt = 1;

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("servers.h:73: error to set socket options\n");
        exit(1);
    }

    struct sockaddr_in serv_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) {
        perror("servers.h:86: error to bind socket\n");
        exit(1);
    }

    if (listen(sockfd, N_BACKLOG) == -1) {
        perror("servers.h:91: error on listen socket\n");
        exit(1);
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
        perror("servers.h:132: error on fcntl to get flags");
        exit(1);
    }

    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("servers.h:139: error to set nonblocking mode on socket connection");
        exit(1);
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
            perror("servers.h:173: error to receive socket data");
            exit(1);
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
            perror("servers.h:228: error to send data on socket");
            exit(1);
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

#endif
