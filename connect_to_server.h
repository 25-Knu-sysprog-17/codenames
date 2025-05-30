#ifndef CONNECT_TO_SERVER_H
#define CONNECT_TO_SERVER_H

#include <sys/socket.h>
#include <stdbool.h>

int connect_to_server(const char* ip, int port);
bool server_connection_successful();
int get_server_socket();

#endif