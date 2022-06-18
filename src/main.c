#include <stdio.h>
#include <stdlib.h>

#include "headers/state_machine.h"
#include "sequential_server.c"
#include "thread_server.c"

int 
main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IONBF, 0);

    int port = 8081;

    if (argc >= 2) {
        port = atoi(argv[1]);
    }

    printf("server listen on port: %d\n", port);

    int sockfd = listen_inet_socket(port);

    /* sequential_server(sockfd); */
    thread_server(sockfd);

    return 0;
}
