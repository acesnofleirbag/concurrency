#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "headers/servers.h"

void
nonblocking_sock_connection(int sockfd) {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);

    int sockfd_new = accept(sockfd, (struct sockaddr*) &peer_addr, &peer_addr_len);

    if (sockfd_new == -1) {
        perror("nonblocking_sock_connection.c:10: error to accept connection");
        exit(1);
    }

    log_peer_connection(&peer_addr, peer_addr_len);

    // NOTE: setting nonblock mode on the socket
    int flags = fcntl(sockfd_new, F_GETFL, 0);

    if (flags == -1) {
        perror("nonblocking_sock_connection.c:23: error on fcntl to get flags");
        exit(1);
    }

    if (fcntl(sockfd_new, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("nonblocking_sock_connection.c:30: error to set nonblocking mode on socket connection");
        exit(1);
    }

    while (1) {
        uint8_t buf[1024];

        printf("calling recv...\n");

        int len = recv(sockfd_new, buf, sizeof(buf), 0);

        if (len == -1) {
            // NOTE: no data on the socket, sleep a bit and retry recv()
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(500 * 1000);
                continue;
            }
        } else if (len == 0) {
            printf("peer disconnected, i'm done\n");
            break;
        }

        printf("recv returned %d bytes\n", len);
    }

    close(sockfd_new);
    close(sockfd);
}
