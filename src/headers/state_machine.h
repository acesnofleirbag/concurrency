#ifndef HEADERS_STATE_MACHINE_H
#define HEADERS_STATE_MACHINE_H

/*
 * --------------------
 * STATE MACHINE FORMAT
 * --------------------
 *
 *             START
 *               |
 *               |
 *               v
 *       +----------------+
 *       |                |
 *       |    WAIT FOR    |
 *       |    CLIENT      |
 *       |                |
 *       +-------+--------+                       IN: RECEIVED CHAR X (X != $)
 *               |
 * IN: CLIENT    |                                OUT: SEND CHAR (X + 1)
 * CONNECTED     |
 *               |                                +-----------------------+
 * OUT: SEND *   |          IN: RECEIVED ^        |                       |
 *               v                                |                       |
 *       +----------------+               +-------+--------+              |
 *       |                +-------------->|                |              |
 *       |                |               |                |              |
 *       |    WAITTING    |               |   PROCESSING   |<-------------+
 *       |                |               |                |
 *       |                |<--------------+                |
 *       +----------------+               +----------------+
 *                          IN: RECEIVED $
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef enum { INITIAL_ACK, WAITTING, PROCESSING } ProcessingState;

void
start_state_machine(int sockfd) {
    if (send(sockfd, "*", 1, 0) != 1) {
        perror("state_machine.h:20: error to send '*' message on socket\n");
        exit(1);
    }

    ProcessingState state = WAITTING;

    while (1) {
        uint8_t buf[1024];
        int len = recv(sockfd, buf, sizeof(buf), 0);

        if (len < 0) {
            perror("state_machine.h:29: error to receive message on socket\n");
            exit(1);
        } else if (len == 0) {
            break;
        }

        for (int i = 0; i < len; i++) {
            switch (state) {
                case INITIAL_ACK:
                    printf("test");
                    continue;
                case WAITTING:
                    if (buf[i] == '^') {
                        state = PROCESSING;
                    }

                    break;
                case PROCESSING:
                    if (buf[i] == '$') {
                        state = WAITTING;
                    } else {
                        buf[i] += 1;

                        if (send(sockfd, &buf[i], 1, 0) != 1) {
                            perror("state_machine.h:52: error to send message (processing) on socket\n");
                            close(sockfd);

                            return;
                        }
                    }

                    break;
            }
        }
    }

    close(sockfd);
}

#endif
