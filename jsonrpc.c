#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "jsonrpc.h"

struct ev_loop *loop;

static void close_connection(struct ev_loop *loop, ev_io *w)
{
    struct jrpc_connection *conn = container_of(w, struct jrpc_connection, io);
    ev_io_stop(loop, w);
    close(conn->fd);
    free(conn->buffer);
    free(conn);
}

static int send_response(struct jrpc_connection *conn, char *response)
{
    ssize_t write_bytes;
    write_bytes = write(conn->fd, response, strlen(response));
    if (write_bytes > 0) {
        write_bytes = write(conn->fd, "\n", 1);
        return 0;
    } else {
        return -1;
    }
}

static int send_error(struct jrpc_connection *conn, int code, const char *message, cJSON* id)
{
    int ret = 0;
    cJSON *result_root = cJSON_CreateObject();
    cJSON *error_root = cJSON_CreateObject();
    cJSON_AddNumberToObject(error_root, "code", code);
    cJSON_AddStringToObject(error_root, "message", message);
    cJSON_AddItemToObject(result_root, "error", error_root);
    cJSON_AddItemToObject(result_root, "id", id);
    char *str_result = cJSON_Print(result_root);
    ret = send_response(conn, str_result);
    free(str_result);
    cJSON_Delete(result_root);
    return ret;
}

static int eval_request(struct jrpc_connection *conn, cJSON *root)
{
    cJSON *method, *params, *id;
    method = cJSON_GetObjectItem(root, "method");
    if (method != NULL && method->type == cJSON_String) {
        // TBD: invoke_procedure();
        return 0;
    }
    send_error(conn, JRPC_INVALID_REQUEST, "The JSON sent is not a valid Request object.", NULL);
    return -1;
}

static void connection_cb(struct ev_loop *loop, ev_io *w, int revents)
{
    struct jrpc_connection *conn = container_of(w, struct jrpc_connection, io);
    int max_read_size;
    size_t bytes_read = 0;
    int fd = conn->fd;

    if (conn->pos == (conn->buffer_size - 1)) {
        char *new_buf = realloc(conn->buffer, conn->buffer_size*=2);
        if (new_buf == NULL) {
            perror("Memory error");
            return close_connection(loop, w);
        }
        conn->buffer = new_buf;
        memset(conn->buffer + conn->pos, 0, conn->buffer_size - conn->pos);
    }

    max_read_size = conn->buffer_size - conn->pos - 1;
    if ((bytes_read = read(fd, conn->buffer + conn->pos, max_read_size)) == -1) {
        perror("read()");
        return close_connection(loop, w);
    }
    if (!bytes_read) {
        printf("no data now\n");
        return close_connection(loop, w);
    } else {
        printf("got: %s\n", conn->buffer);
        conn->pos += bytes_read;
        cJSON *root;
        const char *end_ptr = NULL;

        if ((root = cJSON_ParseWithOpts(conn->buffer, &end_ptr, cJSON_False)) != NULL) {
            char *str_result = cJSON_Print(root);
            printf("Got JSON: \n%s\n", str_result);
            free(str_result);

            if (root->type == cJSON_Object) {
                eval_request(conn, root);
            }

            cJSON_Delete(root);
        } else {
            if (end_ptr != (conn->buffer + conn->pos)) {
                printf("Invalid JSON data: \n---\n%s\n---\n", conn->buffer);
            }
            send_error(conn, JRPC_PARSE_ERROR, "Parse error. Invalid JSON was received by the server.", NULL);
            return close_connection(loop, w);
        }

    }
}

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents)
{
    struct jrpc_server *server = w->data;
    struct jrpc_connection *conn = malloc(sizeof(struct jrpc_connection));
    struct sockaddr_in their_addr;
    socklen_t sin_size = sizeof(their_addr);

    conn->fd = accept(w->fd, (struct sockaddr *)&their_addr, &sin_size);
    if (conn->fd == -1) {
        perror("accept()");
        free(conn);
        return;
    }
    printf("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));

    ev_io_init(&conn->io, connection_cb, conn->fd, EV_READ);
    conn->server = server;
    conn->buffer_size = 1024;
    conn->buffer = malloc(conn->buffer_size);
    conn->pos = 0;
    ev_io_start(loop, &conn->io);
}

static int __jrpc_server_start(struct jrpc_server *server)
{
    int fd;
    int val = 1;
    struct sockaddr_in laddr;

    fd = socket(AF_INET, SOCK_STREAM, 0 /* equal to IPPROTO_TCP */);
    if (fd < 0) {
        perror("socket()");
        exit(1);
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        perror("setsockopt()");
        exit(1);
    }

    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(server->port);
    inet_pton(AF_INET, "0.0.0.0", &laddr.sin_addr);
    if (bind(fd, (void *)&laddr, sizeof(laddr)) < 0) {
        perror("bind()");
        exit(1);
    }

    if (listen(fd, 200) < 0) {
        perror("listen()");
        exit(1);
    }

    ev_io_init(&server->listen_watcher, accept_cb, fd, EV_READ);
    server->listen_watcher.data = server;
    ev_io_start(server->loop, &server->listen_watcher);

    return 0;
}

int jrpc_server_init(struct jrpc_server *server, int port)
{
    loop = EV_DEFAULT;
    memset(server, 0, sizeof(struct jrpc_server));
    server->loop = loop;
    server->port = port;

    return __jrpc_server_start(server);
}

void jrpc_dump_procedure(struct jrpc_server *server)
{
    int i;

    for (i = 0; i < server->procedure_count; i++){
        printf("%s\n", server->procedures[i].name);
    }
}

int jrpc_register_procedure(struct jrpc_server *server, jrpc_function func, char *name, void *data)
{
    int i;

    if (name == NULL) {
        return -1;
    }

    // if already, then just replace
    for (i = 0; i < server->procedure_count; i++){
        if(!strcmp(name, server->procedures[i].name)){
            if (server->procedures[i].name)
                free(server->procedures[i].name);
            if ((server->procedures[i].name = strdup(name)) == NULL)
                return -1;
            
            server->procedures[i].function = func;
            server->procedures[i].data = data;
            return 0;
        }
    }

    // Add a new one
    i = server->procedure_count++;
    if (!server->procedures) {
        server->procedures = malloc(sizeof(struct jrpc_procedure));
    } else {
        struct jrpc_procedure *p = realloc(server->procedures, 
                    sizeof(struct jrpc_procedure) * server->procedure_count);
        if (!p)
            return -1;
        server->procedures = p;
    }

    if ((server->procedures[i].name = strdup(name)) == NULL)
        return -1;
    
    server->procedures[i].function = func;
    server->procedures[i].data = data;
    return 0;
}

int jrpc_deregister_procedure(struct jrpc_server *server, char *name)
{
    int i, found = 0;
    if (name == NULL)
        return -1;
        
    if (server->procedures) {
        for (i=0; i<server->procedure_count; i++) {
            if (found)
                server->procedures[i-1] = server->procedures[i];
            else if (!strcmp(name, server->procedures[i].name)) {
                found = 1;
                free(server->procedures[i].name);
                server->procedures[i].name = NULL;
            }
        }
        if (found) {
            server->procedure_count--;
            if (server->procedure_count) {
                struct jrpc_procedure *p = realloc(server->procedures,
                    sizeof(struct jrpc_procedure) * server->procedure_count);
                if (!p) {
                    perror("realloc");
                    return -1;
                }
                server->procedures = p;
            } else {
                server->procedures = NULL;
            }
        }
    } else {
        fprintf(stderr, "server: procedure '%s' not found \n", name);
        return -1;
    }
    return 0;
}

void jrpc_server_run(struct jrpc_server *server)
{
    ev_run(server->loop, 0);
}

void jrpc_server_destroy(struct jrpc_server *server)
{

}