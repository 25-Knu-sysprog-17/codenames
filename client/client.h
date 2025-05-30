#ifndef CLIENT_H
#define CLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <openssl/ssl.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8089
#define CLIENT_MAX_INPUT 32
#define TOKEN_LEN 64

extern SSL *ssl;
extern char token[TOKEN_LEN];

#endif
