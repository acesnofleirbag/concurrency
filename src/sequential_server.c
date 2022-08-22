#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "headers/error.h"
#include "headers/servers.h"
#include "headers/state_machine.h"

void
sequential_server(int sockfd) {
    while (1) {
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);

        int sockfd_new = accept(sockfd, (struct sockaddr*) &peer_addr, &peer_addr_len);

        if (sockfd_new < 0) {
            errlog("error to accept socket connection");
        }

        log_peer_connection(&peer_addr, peer_addr_len);
        start_state_machine(sockfd_new);

        printf("peer done\n");
    }
}
