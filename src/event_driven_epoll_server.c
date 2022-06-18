// NOTE: this have a better approach otherwise the select because dont need to iterate over all elements to know what
// element has the state modified

#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>

#include "headers/servers.h"

void
event_driven_epoll_server(int sockfd) {
    make_sock_nonblocking(sockfd);

    int epollfd = epoll_create1(0);

    if (epollfd == -1) {
        perror("event_driven_epoll_server.c:13: error to create epoll queue");
        exit(1);
    }

    struct epoll_event accept_event;

    accept_event.data.fd = sockfd;
    accept_event.events = EPOLLIN;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &accept_event) == -1) {
        perror("event_driven_epoll_server.c:23: error on epoll queue manipulation");
        exit(1);
    }

    struct epoll_event* events = calloc(MAXFDS, sizeof(struct epoll_event));

    if (events == NULL) {
        perror("event_driven_epoll_server.c:28: error to alloc memory");
        exit(1);
    }

    while (1) {
        int ready_len = epoll_wait(epollfd, events, MAXFDS, -1);

        for (int i = 0; i < ready_len; i++) {
            if (events[i].events & EPOLLERR) {
                perror("event_driven_epoll_server.c:38: epoll events contains an error");
                exit(1);
            }

            if (events[i].data.fd == sockfd) {
                struct sockaddr_in peer_addr;
                socklen_t peer_addr_len = sizeof(peer_addr);

                int sockfd_new = accept(sockfd, (struct sockaddr*) &peer_addr, &peer_addr_len);

                if (sockfd_new == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("accept returned EAGAIN or EWOULDBLOCK\n");
                    } else {
                        perror("event_driven_epoll_server.c:47: error to accept socket connection");
                        exit(1);
                    }
                } else {
                    make_sock_nonblocking(sockfd_new);

                    if (sockfd_new >= MAXFDS) {
                        fprintf(stderr, "socket fd (%d) >= MAXFDS (%d)", sockfd_new, MAXFDS);
                        exit(1);
                    }

                    fd_status_t status = on_peer_connected(sockfd_new, &peer_addr, peer_addr_len);

                    struct epoll_event event = {0};

                    event.data.fd = sockfd_new;

                    if (status.want_read) {
                        event.events |= EPOLLIN;
                    }

                    if (status.want_write) {
                        event.events |= EPOLLOUT;
                    }

                    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd_new, &event) == -1) {
                        perror("event_driven_epoll_server.c:77: error on epoll queue manipulation");
                        exit(1);
                    }
                }
            } else {
                // NOTE: a peer socket is ready to read
                if (events[i].events & EPOLLIN) {
                    int fd = events[i].data.fd;

                    fd_status_t status = on_peer_ready_recv(fd);

                    struct epoll_event event = {0};

                    event.data.fd = fd;

                    if (status.want_read) {
                        event.events |= EPOLLIN;
                    }

                    if (status.want_write) {
                        event.events |= EPOLLOUT;
                    }

                    if (event.events == 0) {
                        printf("socket %d closing\n", fd);

                        if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) == -1) {
                            perror("event_driven_epoll_server.c:104: error on epoll queue maniputation");
                            exit(1);
                        }

                        close(fd);
                    } else if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) == -1) {
                        perror("event_driven_epoll_server.c:111: error on epoll queue maniputation");
                        exit(1);
                    }
                    // NOTE: a peer socket is ready to write
                } else if (events[i].events & EPOLLOUT) {
                    int fd = events[i].data.fd;

                    fd_status_t status = on_peer_ready_send(fd);

                    struct epoll_event event = {0};

                    event.data.fd = fd;

                    if (status.want_read) {
                        event.events |= EPOLLIN;
                    }

                    if (status.want_write) {
                        event.events |= EPOLLOUT;
                    }

                    if (event.events == 0) {
                        printf("socket %d closing\n", fd);

                        if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) == -1) {
                            perror("event_driven_epoll_server.c:135: error on epoll queue maniputation");
                            exit(1);
                        }

                        close(fd);
                    } else if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) == -1) {
                        perror("event_driven_epoll_server.c:140: error on epoll queue maniputation");
                        exit(1);
                    }
                }
            }
        }
    }
}
