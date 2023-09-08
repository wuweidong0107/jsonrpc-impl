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

struct jrpc_connection {
    int fd;
    int pos;
    struct ev_io io;
    unsigned int buffer_size;
    char *buffer;
};

int jrpc_server_init(struct jrpc_server *server, int port);
int jprc_register_procedure(struct jrpc_server *server, jrpc_function func, char *name, void *data);
int jprc_deregister_procedure(struct jrpc_server *server, char *name);
void jprc_server_run(struct jrpc_server *server);
void jprc_server_destroy(struct jrpc_server *server);
#endif
