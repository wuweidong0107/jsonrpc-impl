#include <stdio.h>
#include <stdlib.h>
#include "jsonrpc.h"

#define PORT 1234
struct jrpc_server my_server;

cJSON *say_hello(jrpc_ctx_t *ctx, cJSON *params, cJSON *id)
{
    return cJSON_CreateString("Hello!");
}

cJSON *add(jrpc_ctx_t *ctx, cJSON *params, cJSON *id)
{
    cJSON *a = cJSON_GetArrayItem(params, 0);
    cJSON *b = cJSON_GetArrayItem(params, 1);
    return cJSON_CreateNumber(a->valueint + b->valueint);
}

int main(int argc, char* argv[])
{
    jrpc_server_init(&my_server, PORT);
    jrpc_register_procedure(&my_server, say_hello, "sayHello", NULL);
    jrpc_register_procedure(&my_server, add, "add", NULL);
    jrpc_dump_procedure(&my_server);
    jrpc_deregister_procedure(&my_server, "add");
    jrpc_dump_procedure(&my_server);
    jrpc_server_run(&my_server);
    return 0;
}