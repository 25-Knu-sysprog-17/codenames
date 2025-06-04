#ifndef CONNECT_TO_SERVER_H
#define CONNECT_TO_SERVER_H

#include <sys/socket.h>
#include <stdbool.h>

bool connect_to_server(const char* ip, int port);
bool server_connection_successful();
int get_server_socket();
char* get_user_nickname();
bool wait_for_match(int* progress_out);

#endif