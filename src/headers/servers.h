#ifndef HEADERS_SERVERS_H
#define HEADERS_SERVERS_H

#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#include "state_machine.h"

#define N_BACKLOG 64

typedef struct { 
    int sockfd;
} thread_config_t;

void sequential_server(int);
void thread_server(int);

int 
listen_inet_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1) {
        perror("servers.h:20: error to start the INET/TCP socket connection\n");
        exit(1);
    }

    int opt = 1;

    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("servers.h:27: error to set socket options\n");
        exit(1);
    }

    struct sockaddr_in serv_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if(bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) {
        perror("servers.h:40: error to bind socket\n");
        exit(1);
    }

    if(listen(sockfd, N_BACKLOG) == -1) {
        perror("servers.h:45: error on listen socket\n");
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

#endif
