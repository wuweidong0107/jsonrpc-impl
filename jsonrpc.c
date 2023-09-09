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

static void close_connection(struct jrpc_connection *conn)
{
    close(conn->fd);
    free(conn->buffer);
    free(conn);
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
            return close_connection(conn);
        }
        conn->buffer = new_buf;
        memset(conn->buffer + conn->pos, 0, conn->buffer_size - conn->pos);
    }

    max_read_size = conn->buffer_size - conn->pos - 1;
    if ((bytes_read = read(fd, conn->buffer + conn->pos, max_read_size)) == -1) {
        perror("read()");
        return close_connection(conn);
    }
    if (!bytes_read) {
        printf("no data now\n");
        return close_connection(conn);
    } else {
        printf("got: %s\n", conn->buffer);
        conn->pos += bytes_read;
        cJSON *root;
        const char *end_ptr = NULL;

        if ((root = cJSON_ParseWithOpts(conn->buffer, &end_ptr, cJSON_False)) != NULL) {
            char *str_result = cJSON_Print(root);
            printf("Got JSON: \n%s\n", str_result);
            free(str_result);
            cJSON_Delete(root);
        } else {
            if (end_ptr != (conn->buffer + conn->pos)) {
                printf("Invalid JSON data: \n---\n%s\n---\n", conn->buffer);
            }
            return close_connection(conn);
        }

    }
}

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents)
{
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

    conn->buffer_size = 1024;
    conn->buffer = malloc(conn->buffer_size);
    conn->pos = 0;
    ev_io_init(&conn->io, connection_cb, conn->fd, EV_READ);
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

int jprc_register_procedure(struct jrpc_server *server, jrpc_function func, char *name, void *data)
{
    return 0;
}

int jprc_deregister_procedure(struct jrpc_server *server, char *name)
{
    return 0;
}

void jprc_server_run(struct jrpc_server *server)
{
    ev_run(server->loop, 0);
}

void jprc_server_destroy(struct jrpc_server *server)
{

}