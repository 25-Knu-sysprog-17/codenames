#include "client.h"
#include "login_screen.h"
#include "lobby_screen.h"
#include "signup_screen.h"
#include "result_screen.h"
#include "gui_invalid_token.h"
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

SSL *ssl = NULL;
char token[TOKEN_LEN] = {0};
int game_sock = -1;

int get_game_sock() { return game_sock; }
void set_game_sock(int s) { game_sock = s; }

int main() {
    // SSL 소켓으로 로그인/회원가입
    setlocale(LC_ALL, ""); // 한글 설정

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SSL_PORT),
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

    SceneState current_scene = SCENE_LOGIN;
    char id[CLIENT_MAX_INPUT] = {0};
    char pw[CLIENT_MAX_INPUT] = {0};
    char nickname[CLIENT_MAX_INPUT] = {0};

    while(1) {

        while (current_scene != SCENE_MAIN) {
            if (current_scene == SCENE_LOGIN) {
                current_scene = login_screen(id, pw);
            } else if (current_scene == SCENE_SIGNUP) {
                current_scene = signup_screen(id, pw, nickname);
            }
        }

        // 로그인/회원가입 후 SSL 소켓 닫기
        SSL_free(ssl);
        close(sock);
        SSL_CTX_free(ctx);

        // TCP 소켓으로 게임 서버(메인 화면)에 접속
        struct sockaddr_in game_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(TCP_PORT),
            .sin_addr.s_addr = inet_addr(SERVER_IP)
        };
        game_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (game_sock == -1) {
            perror("socket");
            return 1;
        }


        if (connect(game_sock, (struct sockaddr*)&game_addr, sizeof(game_addr)) == -1) {
            perror("connect");
            close(game_sock);
            return 1;
        }

        // 메인 화면(게임) 진입
        if(current_scene == SCENE_MAIN)
            current_scene = lobby_screen();
        if(current_scene == SCENE_INVALID_TOKEN)
            show_invalid_token_screen();
        if(current_scene == SCENE_ERROR) {
            printf("error\n");
            break;
        }
        if(current_scene == SCENE_EXIT)
            break;
    }

    // (임시) 게임 결과 화면 호출
    // 이것도 while 문 안에 넣어야 함.
    // int red_score = 3;
    // int blue_score = 9;
    // Result result = RESULT_WIN;
    
    // current_scene = result_function(red_score, blue_score, result);
    // if(current_scene == SCENE_INVALID_TOKEN) {
    //     // 경고창 출력 -> 로그인 화면으로 */
    //     show_invalid_token_screen();
    // }
    // if(current_scene == SCENE_ERROR) {
    //     printf("error\n");
    //     // break;
    // }

    close(game_sock);
    return 0;
}
