#include <stdio.h>
#include <stdlib.h>
#include "jsonrpc.h"

#define PORT 1234
struct jrpc_server my_server;

int main(int argc, char* argv[])
{
    jrpc_server_init(&my_server, PORT);
    exit(0);
}