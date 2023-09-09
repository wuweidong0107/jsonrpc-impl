#ifndef JSONRPC_H_
#define JSONRPC_H_

#include <ev.h>
#include "cJSON.h"

#ifndef container_of
#define container_of(ptr, type, member)					\
	({								\
		const __typeof__(((type *) NULL)->member) *__mptr = (ptr);	\
		(type *) ((char *) __mptr - offsetof(type, member));	\
	})
#endif

#define JRPC_PARSE_ERROR -32700
#define JRPC_INVALID_REQUEST -32600
#define JRPC_METHOD_NOT_FOUND -32601
#define JRPC_INVALID_PARAMS -32603
#define JRPC_INTERNAL_ERROR -32693

typedef struct {
    void *data;
    int error_code;
    char *error_message;
} jrpc_ctx_t;

typedef cJSON* (*jrpc_function)(jrpc_ctx_t *context, cJSON *params, cJSON *id);

struct jrpc_procedure {
    char *name;
    jrpc_function function;
    void *data;
};

struct jrpc_server {
    int port;
    struct  ev_loop *loop;
    ev_io listen_watcher;
    int procedure_count;
    struct jrpc_procedure *procedures;  
};

struct jrpc_connection {
    int fd;
    int pos;
    struct jrpc_server *server;
    struct ev_io io;
    unsigned int buffer_size;
    char *buffer;
};

int jrpc_server_init(struct jrpc_server *server, int port);
int jrpc_register_procedure(struct jrpc_server *server, jrpc_function func, char *name, void *data);
int jrpc_deregister_procedure(struct jrpc_server *server, char *name);
void jrpc_dump_procedure(struct jrpc_server *server);
void jrpc_server_run(struct jrpc_server *server);
void jrpc_server_destroy(struct jrpc_server *server);
#endif
