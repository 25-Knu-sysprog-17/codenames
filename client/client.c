#include "client.h"
#include "login_screen.h"
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8089
#define TOKEN_LEN 64

SSL *ssl = NULL;
char token[TOKEN_LEN] = {0};

int main() {

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = inet_addr(SERVER_IP)
    };

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(sock);
        return 1;
    }

    SSL_library_init();
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        perror("SSL_CTX_new");
        close(sock);
        return 1;
    }

    if (!SSL_CTX_load_verify_locations(ctx, "ca.crt", NULL)) {
        fprintf(stderr, "CA 인증서 로드 실패\n");
        exit(1);
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);


    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);

    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sock);
        SSL_CTX_free(ctx);
        return 1;
    }

    char id[CLIENT_MAX_INPUT] = {0};
    char pw[CLIENT_MAX_INPUT] = {0};
    login_screen(id, pw);
    
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    return 0;
}
