#ifndef JSONRPC_H_
#define JSONRPC_H_

#include <ev.h>
#include "cJSON.h"

typedef struct {
    void *data;
    int error_code;
    char *error_message;
} jprc_ctx_t;

typedef cJSON* (*jrpc_function)(jprc_ctx_t *context, cJSON *params, cJSON *id);

struct jrpc_procedur {
    char *name;
    jrpc_function function;
    void *data;
};

struct jrpc_server {
    int port;
    struct  ev_loop *loop;
    ev_io listen_watcher;
    struct jprc_procedure *procedures;  
};

int jrpc_server_init(struct jrpc_server *server, int port);

#endif
