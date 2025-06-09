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
#include "user_info.h"
#include "game_result.h"
#include "room_manager.h"

#define SERVER_PORT 55014
#define TCP_PORT 55015
#define MAX_CLIENTS 64
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

// SSL 클라이언트 처리 함수 (쓰레드)
void *handle_ssl_client(void *arg) {
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
                add_token_to_db_after_removal(id, token);
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
                add_token_to_db_after_removal(id, token);
                // 클라이언트에 토큰 전송 (예: "SIGNUP_OK|토큰값")
                char response[TOKEN_LEN + 16];
                snprintf(response, sizeof(response), "LOGIN_OK|%s", token);
                SSL_write(ssl, response, strlen(response));
            } else if (result == -2) { // 계정 없음
                SSL_write(ssl, "LOGIN_NO_ACCOUNT", strlen("LOGIN_NO_ACCOUNT"));
            } else if (result == -3) { // 비밀번호 불일치
                SSL_write(ssl, "LOGIN_WRONG_PW", strlen("LOGIN_WRONG_PW"));
            } else if (result == -4) { // 정지된 계정
                SSL_write(ssl, "LOGIN_SUSPENDED", strlen("LOGIN_SUSPENDED"));
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

// TCP 클라이언트 처리 함수 (쓰레드)
void *handle_tcp_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    int len;

    while ((len = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[len] = '\0';
        printf("TCP 수신: %s\n", buffer);
        char nickname[64];
        char user_id[BUFFER_SIZE];
        int wins = 0, losses = 0;

        if (strncmp(buffer, "TOKEN|", 6) == 0) { // 토큰 인증 및 결과 전송
            char *token = buffer + 6;
            if (get_user_info_by_token(token, user_id, sizeof(user_id), nickname, sizeof(nickname), &wins, &losses) == 0) {
                char response[256];
                snprintf(response, sizeof(response), "NICKNAME|%s|WINS|%d|LOSSES|%d", nickname, wins, losses);
                send(client_sock, response, strlen(response), 0);
            } else {
                send(client_sock, "INVALID_TOKEN", 13, 0);
            }
        } else if (strncmp(buffer, "CMD|QUERY_WAIT|", 15) == 0) {
            char *token = buffer + 15;
            
            handle_waiting_command(client_sock, token);
            client_response_loop(client_sock, token);           
        } else if (strncmp(buffer, "RESULT|", 7) == 0) { // 결과 저장
            // 예시: "RESULT|토큰값|WIN" 또는 "RESULT|토큰값|LOSS"
            char *token = buffer + 7;
            char *result = strchr(token, '|');
            printf("result\n");
            if (!result) {
                send(client_sock, "ERROR", 5, 0);
                continue;
            }
            *result++ = '\0';
            if (get_user_info_by_token(token, user_id, sizeof(user_id), nickname, sizeof(nickname), &wins, &losses) == 0) {
                // game_stats 테이블에 결과 반영 (예: WIN이면 wins+1)
                int save_result = save_game_result(token, result);
                if (save_result == 0) {
                    // 성공
                    send(client_sock, "RESULT_OK", 9, 0);
                    // printf("Game result saved successfully for token: %s, result: %s\n", token, result);
                } else {
                    // 실패 (토큰 무효, DB 오류 등)
                    send(client_sock, "RESULT_ERROR", 12, 0);
                    // printf("Failed to save game result for token: %s, result: %s\n", token, result);
                }
            } else {
                send(client_sock, "INVALID_TOKEN", 13, 0);
            }
        } else {
            send(client_sock, "ERROR", 5, 0);
        }
    }

    printf("TCP 클라이언트 연결 종료\n");
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

    // SSL 서버 소켓 생성
    int ssl_server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in ssl_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(SERVER_PORT)
    };

    int optval = 1;
    setsockopt(ssl_server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(ssl_server_sock, (struct sockaddr *)&ssl_addr, sizeof(ssl_addr)) == -1) {
        perror("bind");
        close(ssl_server_sock);
        exit(EXIT_FAILURE);
    }
    if (listen(ssl_server_sock, MAX_CLIENTS) == -1) {
        perror("listen");
        close(ssl_server_sock);
        exit(EXIT_FAILURE);
    }

    // TCP 서버 소켓 생성
    int tcp_server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in tcp_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(TCP_PORT)
    };

    setsockopt(tcp_server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(tcp_server_sock, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) == -1) {
        perror("bind");
        close(tcp_server_sock);
        exit(EXIT_FAILURE);
    }
    if (listen(tcp_server_sock, MAX_CLIENTS) == -1) {
        perror("listen");
        close(tcp_server_sock);
        exit(EXIT_FAILURE);
    }

    printf("SSL 서버가 %d 포트에서 대기 중...\n", SERVER_PORT);
    printf("TCP 서버가 %d 포트에서 대기 중...\n", TCP_PORT);

    // 클라이언트 연결 처리
    while (1) {
        // --- SSL 클라이언트 처리 ---
        struct sockaddr_in ssl_client_addr;
        socklen_t ssl_client_len = sizeof(ssl_client_addr);
        int ssl_client_sock = accept(ssl_server_sock, (struct sockaddr *)&ssl_client_addr, &ssl_client_len);
        if (ssl_client_sock > 0) {
            printf("SSL 클라이언트 연결됨: %s:%d\n",
                inet_ntoa(ssl_client_addr.sin_addr), ntohs(ssl_client_addr.sin_port));
            int *ssl_sock_ptr = malloc(sizeof(int));
            if (!ssl_sock_ptr) {
                perror("malloc");
                close(ssl_client_sock);
                continue;
            }
            *ssl_sock_ptr = ssl_client_sock;

            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_ssl_client, (void *)ssl_sock_ptr) != 0) {
                perror("pthread_create");
                close(ssl_client_sock);
                free(ssl_sock_ptr);
            } else {
                pthread_detach(thread_id);
            }
        }

        // --- TCP 클라이언트 처리 ---
        struct sockaddr_in tcp_client_addr;
        socklen_t tcp_client_len = sizeof(tcp_client_addr);
        int tcp_client_sock = accept(tcp_server_sock, (struct sockaddr *)&tcp_client_addr, &tcp_client_len);
        if (tcp_client_sock > 0) {
            printf("TCP 클라이언트 연결됨: %s:%d\n",
                inet_ntoa(tcp_client_addr.sin_addr), ntohs(tcp_client_addr.sin_port));
            int *tcp_sock_ptr = malloc(sizeof(int));
            if (!tcp_sock_ptr) {
                perror("malloc");
                close(tcp_client_sock);
                continue;
            }
            *tcp_sock_ptr = tcp_client_sock;

            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, handle_tcp_client, (void *)tcp_sock_ptr) != 0) {
                perror("pthread_create");
                close(tcp_client_sock);
                free(tcp_sock_ptr);
            } else {
                pthread_detach(thread_id);
            }
        }
    }

    close(ssl_server_sock);
    close(tcp_server_sock);
    SSL_CTX_free(ssl_ctx);
    cleanup_openssl();
    return 0;
}