#include "connect_to_server.h"
#include <ncurses.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

static bool connected = false;
static int server_sock = -1;

int connect_to_server(const char* ip, int port) {
    if (connected) return server_sock;  // 이미 연결되어 있으면 그대로 반환

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    if (connect(server_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        connected = false;
        return -1;
    }

    connected = true;
    return server_sock;
}

bool server_connection_successful() {
    return connected;
}

int get_server_socket() {
    return connected ? server_sock : -1;
}
