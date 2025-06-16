#include "client.h"
#include "login_screen.h"
#include "lobby_screen.h"
#include "signup_screen.h"
#include "result_screen.h"
#include "gui_invalid_token.h"
#include "codenames_screen.h"
#include <stdio.h>
#include <locale.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define SERVER_IP "127.0.0.1"
#define SSL_PORT 55014
#define TCP_PORT 55015
#define TOKEN_LEN 64

// 전역 변수들
int ssl_sock = -1;
SSL *ssl = NULL;
SSL_CTX *ctx = NULL;
int game_sock = -1;
char token[TOKEN_LEN] = {0};

int get_game_sock() { return game_sock; }
void set_game_sock(int s) { game_sock = s; }

SSL* get_ssl() { return ssl; }

// SSL 정리 함수
void cleanup_ssl() {
    if (ssl) {
        SSL_free(ssl);
        ssl = NULL;
    }
    if (ssl_sock != -1) {
        close(ssl_sock);
        ssl_sock = -1;
    }
    if (ctx) {
        SSL_CTX_free(ctx);
        ctx = NULL;
    }
}

// SSL 연결 초기화 함수
int init_ssl_connection() {
    // 이미 연결되어 있으면 재사용
    if (ssl && ssl_sock != -1) {
        return 0;
    }
    
    SSL_library_init();
    if (!ctx) {
        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) return -1;
    }
    
    struct sockaddr_in ssl_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SSL_PORT),
        .sin_addr.s_addr = inet_addr(SERVER_IP)
    };
    
    ssl_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ssl_sock == -1) return -1;
    
    if (connect(ssl_sock, (struct sockaddr*)&ssl_addr, sizeof(ssl_addr)) == -1) {
        close(ssl_sock);
        ssl_sock = -1;
        return -1;
    }
    
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, ssl_sock);
    
    if (SSL_connect(ssl) <= 0) {
        cleanup_ssl();
        return -1;
    }
    
    return 0;
}

// TCP 게임 서버 연결 초기화 함수
int init_game_connection() {
    // 이미 연결되어 있으면 재사용
    if (game_sock != -1) {
        return 0;
    }
    
    struct sockaddr_in game_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(TCP_PORT),
        .sin_addr.s_addr = inet_addr(SERVER_IP)
    };
    
    game_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (game_sock == -1) return -1;
    
    if (connect(game_sock, (struct sockaddr*)&game_addr, sizeof(game_addr)) == -1) {
        close(game_sock);
        game_sock = -1;
        return -1;
    }
    
    return 0;
}

// 게임 소켓 정리 함수
void cleanup_game_socket() {
    if (game_sock != -1) {
        close(game_sock);
        game_sock = -1;
    }
}

// 모든 연결 정리 함수
void cleanup_all_connections() {
    cleanup_game_socket();
    cleanup_ssl();
}

int main() {
    setlocale(LC_ALL, ""); // 한글 설정

    SceneState current_scene = SCENE_LOGIN;
    char id[CLIENT_MAX_INPUT] = {0};
    char pw[CLIENT_MAX_INPUT] = {0};
    char nickname[CLIENT_MAX_INPUT] = {0};

    // SSL 연결 초기화 (로그인/회원가입 및 닉네임 수정용)
    if (init_ssl_connection() == -1) {
        printf("SSL 연결 실패\n");
        return 1;
    }

    // 메인 게임 루프
    while (1) {
        switch(current_scene) {
            case SCENE_LOGIN:
                current_scene = login_screen(id, pw);
                break;
                
            case SCENE_SIGNUP:
                current_scene = signup_screen(id, pw, nickname);
                break;
                
            case SCENE_MAIN:
                // 메인 화면 진입 시 게임 서버 연결 확인
                if (game_sock == -1) {
                    if (init_game_connection() == -1) {
                        printf("게임 서버 연결 실패\n");
                        current_scene = SCENE_ERROR;
                        break;
                    }
                }
                current_scene = lobby_screen();
                break;  
            case SCENE_RESULT:
                //printf("%d %d %d %d", redScore, blueScore, result, winner_team);
                current_scene = result_function(redScore, blueScore, result, winner_team);
                break;    
            case SCENE_INVALID_TOKEN:
                show_invalid_token_screen();
                // 토큰이 유효하지 않으면 게임 소켓만 정리하고 다시 로그인
                cleanup_game_socket();
                current_scene = SCENE_LOGIN;
                break;
                
            case SCENE_ERROR:
                printf("error\n");
                cleanup_all_connections();
                return 1;
                
            case SCENE_EXIT:
                cleanup_all_connections();
                return 0;
                
            default:
                printf("알 수 없는 scene: %d\n", current_scene);
                current_scene = SCENE_ERROR;
                break;
        }
    }

    // 정리 작업 (도달하지 않을 코드이지만 안전장치)
    cleanup_all_connections();
    return 0;
}