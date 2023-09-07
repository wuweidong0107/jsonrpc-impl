#include "jsonrpc.h"

struct ev_loop *loop;
int jrpc_server_init(struct jrpc_server *server, int port)
{
    loop = EV_DEFAULT;

    memset(server, 0, sizeof(struct jrpc_server));
    server->loop = loop;

    //ev_io_init(&server->listen_watcher, );
    ev_io_start(server->loop, &server->listen_watcher);
    return 0;
}