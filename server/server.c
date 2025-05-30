#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "user_auth.h"
#include "handler.h"
#include "session.h" 

#define SERVER_PORT 8089
#define MAX_CLIENTS 5
#define BUFFER_SIZE 256
#define TOKEN_LEN 64

SSL_CTX *ssl_ctx = NULL;
char token[TOKEN_LEN] = {0};

void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}

SSL_CTX *create_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

// 클라이언트 처리 함수 (쓰레드)
void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    SSL *ssl = SSL_new(ssl_ctx);
    SSL_set_fd(ssl, client_sock);

    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(client_sock);
        return NULL;
    }

    char buffer[BUFFER_SIZE];
    int len;

    while ((len = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[len] = '\0';
        printf("수신: %s\n", buffer);

        if (strncmp(buffer, "CHECK_ID|", 9) == 0) { // ID 중복 검사
            char *id = buffer + 9;
            int is_duplicate = check_id_duplicate(id);

            if (is_duplicate == 1) {
                SSL_write(ssl, "ID_TAKEN", 8);
            } else if (is_duplicate == 0) {
                SSL_write(ssl, "ID_AVAILABLE", 12);
            } else {
                SSL_write(ssl, "ERROR", 5);
            }
        }
        else if (strncmp(buffer, "CHECK_NICKNAME|", 15) == 0) { // 닉네임 중복 검사
            char *nickname = buffer + 15;
            int is_duplicate = check_nickname_duplicate(nickname);
            if (is_duplicate == 1) {
                SSL_write(ssl, "NICKNAME_TAKEN", 14);
            } else if (is_duplicate == 0) {
                SSL_write(ssl, "NICKNAME_AVAILABLE", 18);
            } else {
                SSL_write(ssl, "ERROR", 5);
            }
        }
        else if (strncmp(buffer, "SIGNUP|", 7) == 0) { // 회원가입
            // 요청 파싱 (예: "SIGNUP|아이디|비밀번호|닉네임")
            char *id = buffer + 7;
            char *pw = strchr(id, '|');
            if (!pw) { SSL_write(ssl, "ERROR", 5); continue; }
            *pw++ = '\0';
            char *nickname = strchr(pw, '|');
            if (!nickname) { SSL_write(ssl, "ERROR", 5); continue; }
            *nickname++ = '\0';

            // 회원가입 처리
            int result = signup_user(id, pw, nickname);
            if (result == 0) {
                // 토큰 생성
                char token[TOKEN_LEN];
                generate_token(token, TOKEN_LEN);
                add_token_to_db(id, token);
                // 클라이언트에 토큰 전송 (예: "SIGNUP_OK|토큰값")
                char response[TOKEN_LEN + 16];
                snprintf(response, sizeof(response), "SIGNUP_OK|%s", token);
                SSL_write(ssl, response, strlen(response));
            } else {
                SSL_write(ssl, "SIGNUP_ERROR", strlen("SIGNUP_ERROR"));
            }
        } else if (strncmp(buffer, "LOGIN|", 6) == 0) { // 로그인
            // 요청 파싱 (예: "LOGIN|아이디|비밀번호")
            char *id = buffer + 6;
            char *pw = strchr(id, '|');
            if (!pw) {
                SSL_write(ssl, "ERROR", 5);
                continue;
            }
            *pw++ = '\0';

            // 로그인 처리
            int result = login_user_db(id, pw);
            if (result == 0) {
                // 토큰 생성
                char token[TOKEN_LEN];
                generate_token(token, TOKEN_LEN);
                // 토큰을 DB에 저장
                add_token_to_db(id, token);
                // 클라이언트에 토큰 전송 (예: "SIGNUP_OK|토큰값")
                char response[TOKEN_LEN + 16];
                snprintf(response, sizeof(response), "LOGIN_OK|%s", token);
                SSL_write(ssl, response, strlen(response));
            } else if (result == -2) { // 계정 없음
                SSL_write(ssl, "LOGIN_NO_ACCOUNT", strlen("LOGIN_NO_ACCOUNT"));
            } else if (result == -3) { // 비밀번호 불일치
                SSL_write(ssl, "LOGIN_WRONG_PW", strlen("LOGIN_WRONG_PW"));
            } else { // 기타 오류
                SSL_write(ssl, "LOGIN_ERROR", strlen("LOGIN_ERROR"));
            }
        } else {
            SSL_write(ssl, "ERROR", 5);
        }
    }

    printf("클라이언트 연결 종료\n");
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_sock);
    return NULL;
}

int main() {
    if (init_db() != 0) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;  // 서버 실행 중단
    }

    init_openssl();
    ssl_ctx = create_context();

    if (SSL_CTX_use_certificate_file(ssl_ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (!SSL_CTX_check_private_key(ssl_ctx)) {
        fprintf(stderr, "Private key does not match the certificate\n");
        exit(EXIT_FAILURE);
    }

    int server_sock, *client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    // 소켓 생성
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 서버 주소 설정
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // 소켓 바인딩
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // 연결 대기
    if (listen(server_sock, MAX_CLIENTS) == -1) {
        perror("listen");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("서버가 %d 포트에서 대기 중...\n", SERVER_PORT);

    // 클라이언트 연결 처리
    while (1) {
        client_sock = malloc(sizeof(int));
        if (client_sock == NULL) {
            perror("malloc");
            continue;
        }

        *client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (*client_sock == -1) {
            perror("accept");
            free(client_sock);
            continue;
        }

        printf("클라이언트 연결됨: %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // 쓰레드 생성
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client_sock) != 0) {
            perror("pthread_create");
            close(*client_sock);
            free(client_sock);
        } else {
            pthread_detach(thread_id); // 쓰레드 분리 (종료 시 자동 정리)
        }
    }

    close(server_sock);
    SSL_CTX_free(ssl_ctx);
    cleanup_openssl();
    return 0;
}
