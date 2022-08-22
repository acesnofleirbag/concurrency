#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "headers/error.h"
#include "headers/servers.h"

void
event_driven_select_server(int sockfd) {
    make_sock_nonblocking(sockfd);

    if (sockfd >= FD_SETSIZE) {
        errlog("peer socket connection on fd (%d) >= FD_SETSIZE (%d)", sockfd, FD_SETSIZE);
    }

    fd_set master_read_fd, master_write_fd;

    // NOTE: tracked fd's for write/ write modes. Owned by the event loop
    FD_ZERO(&master_read_fd);
    FD_ZERO(&master_write_fd);

    // NOTE: always monitoring the open socket to detect new connections
    FD_SET(sockfd, &master_read_fd);

    // NOTE: improve iteration efficiency
    int fdset_max = sockfd;

    while (1) {
        // NOTE: select call modify the state, because this we get a copy of the state
        fd_set read_fd_copy = master_read_fd, write_fd_copy = master_write_fd;

        int ready_len = select(fdset_max + 1, &read_fd_copy, &write_fd_copy, NULL, NULL);

        if (ready_len == -1) {
            errlog("error on select get ready state");
        }

        for (int fd = 0; fd <= fdset_max && ready_len > 0; fd++) {
            // NOTE: verify if the fd becomes readable
            if (FD_ISSET(fd, &read_fd_copy)) {
                ready_len--;

                // NOTE: the listening socket is ready, so a new peer is connecting
                if (fd == sockfd) {
                    struct sockaddr_in peer_addr;
                    socklen_t peer_addr_len = sizeof(peer_addr);

                    int sockfd_new = accept(sockfd, (struct sockaddr*) &peer_addr, &peer_addr_len);

                    if (sockfd_new == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // NOTE: can happen due to nonblocking socker mode, is not an error but it's a extremely
                            // rare event
                            printf("accept returned EAGAIN or EWOULDBLOCK\n");
                        } else {
                            errlog("error to accept socket connection");
                        }
                    } else {
                        make_sock_nonblocking(sockfd_new);

                        if (sockfd_new > fdset_max) {
                            if (sockfd_new >= FD_SETSIZE) {
                                errlog("socket fd (%d) >= FD_SETSIZE (%d)", sockfd_new, FD_SETSIZE);
                            }

                            fdset_max = sockfd_new;
                        }
                    }

                    fd_status_t status = on_peer_connected(sockfd_new, &peer_addr, peer_addr_len);

                    if (status.want_read) {
                        FD_SET(sockfd_new, &master_read_fd);
                    } else {
                        FD_CLR(sockfd_new, &master_read_fd);
                    }

                    if (status.want_write) {
                        FD_SET(sockfd_new, &master_write_fd);
                    } else {
                        FD_CLR(sockfd_new, &master_write_fd);
                    }
                } else {
                    fd_status_t status = on_peer_ready_recv(fd);

                    if (status.want_read) {
                        FD_SET(fd, &master_read_fd);
                    } else {
                        FD_CLR(fd, &master_read_fd);
                    }

                    if (status.want_write) {
                        FD_SET(fd, &master_write_fd);
                    } else {
                        FD_CLR(fd, &master_write_fd);
                    }

                    if (!status.want_read && !status.want_write) {
                        printf("socket %d closing\n", fd);
                        close(fd);
                    }
                }
            }

            if (FD_ISSET(fd, &write_fd_copy)) {
                ready_len--;

                fd_status_t status = on_peer_ready_send(fd);

                if (status.want_read) {
                    FD_SET(fd, &master_read_fd);
                } else {
                    FD_CLR(fd, &master_read_fd);
                }

                if (status.want_write) {
                    FD_SET(fd, &master_write_fd);
                } else {
                    FD_CLR(fd, &master_write_fd);
                }

                if (!status.want_read && !status.want_write) {
                    printf("socket %d closing\n", fd);
                    close(fd);
                }
            }
        }
    }
}
