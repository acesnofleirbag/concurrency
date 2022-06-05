#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "headers/servers.h"
#include "headers/state_machine.h"

void 
sequential_server(int sockfd) {
    while(1) {
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);

        int sockfd_new = accept(sockfd, (struct sockaddr*) &peer_addr, &peer_addr_len);

        if (sockfd_new < 0) {
            perror("sequential_server.h:16: herror to accept socket connection\n");
            exit(1);
        }

        log_peer_connection(&peer_addr, peer_addr_len);
        start_state_machine(sockfd_new);
        printf("peer done\n");
    }
}
