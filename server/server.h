#ifndef SERV_H
#define SERV_H

#include <openssl/ssl.h>

// 서버 설정
#define SERVER_PORT 8085
#define MAX_CLIENTS 5
#define BUFFER_SIZE 256
#define TOKEN_LEN 64

// 전역 변수 외부 선언
extern SSL_CTX *ssl_ctx;
extern char token[TOKEN_LEN];

// OpenSSL 초기화 및 정리
void init_openssl(void);
void cleanup_openssl(void);
SSL_CTX *create_context(void);

// 클라이언트 처리 함수
void *handle_client(void *arg);

#endif // SERVER_H
