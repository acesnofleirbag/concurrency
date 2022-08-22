#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#include "headers/error.h"
#include "headers/servers.h"

int
event_driven_libuv_server(int port) {
    int rc;

    uv_tcp_t server_stream;

    if ((rc = uv_tcp_init(uv_default_loop(), &server_stream)) == -1) {
        errlog("libuv tcp connection initialization failed: %s", uv_strerror(rc));
    }

    struct sockaddr_in server_address;

    if ((rc = uv_ip4_addr("0.0.0.0", port, &server_address)) == -1) {
        errlog("libuv server address setup failed: %s", uv_strerror(rc));
    }

    if ((rc = uv_tcp_bind(&server_stream, (const struct sockaddr*) &server_address, 0)) == -1) {
        errlog("libuv bind failed: %s", uv_strerror(rc));
    }

    if ((rc = uv_listen((uv_stream_t*) &server_stream, N_BACKLOG, uv_on_peer_connected)) == -1) {
        errlog("libuv error to listen: %s", uv_strerror(rc));
    }

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    return uv_loop_close(uv_default_loop());
}
