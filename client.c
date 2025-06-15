#include "client.h"
#include "login_screen.h"
#include "lobby_screen.h"
#include "signup_screen.h"
#include "result_screen.h"
#include "gui_invalid_token.h"
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <locale.h>

#define SERVER_IP "127.0.0.1"
#define SSL_PORT 55014
#define TCP_PORT 55015
#define TOKEN_LEN 64

SSL *ssl = NULL;
char token[TOKEN_LEN] = {0};
int game_sock = -1;

// 
typedef enum {
    REPORT_SUCCESS,
    REPORT_ERROR,
    REPORT_INVALID_TOKEN,
    REPORT_USER_NOT_FOUND,
    REPORT_SUSPENDED
} ReportResult;

ReportResult send_report_request(const char *nickname) {
    char request[256];
    snprintf(request, sizeof(request), "REPORT|%s|%s", token, nickname);

    if (SSL_write(ssl, request, strlen(request)) <= 0) {
        perror("SSL_write");
        return REPORT_ERROR;
    }

    char response[256] = {0};
    int recv_len = SSL_read(ssl, response, sizeof(response)-1);
    if (recv_len <= 0) {
        perror("SSL_read");
        return REPORT_ERROR;
    }
    response[recv_len] = '\0';

    // 응답 파싱
    if (strstr(response, "REPORT_OK")) {
        if (strstr(response, "SUSPENDED")) {
            return REPORT_SUSPENDED;
        }
        return REPORT_SUCCESS;
    } else if (strstr(response, "INVALID_TOKEN")) {
        return REPORT_INVALID_TOKEN;
    } else if (strstr(response, "USER_NOT_FOUND")) {
        return REPORT_USER_NOT_FOUND;
    } else {
        return REPORT_ERROR;
    }
}


int main() {
    setlocale(LC_ALL, "");
    // SSL 소켓으로 로그인/회원가입/닉네임 수정
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

        if(current_scene == SCENE_MAIN)
            current_scene = lobby_screen();
        if(current_scene == SCENE_INVALID_TOKEN) {
            show_invalid_token_screen();
            current_scene = login_screen(id, pw);
            if (current_scene == SCENE_SIGNUP) {
                current_scene = signup_screen(id, pw, nickname);
            }
        }
        if(current_scene == SCENE_ERROR) {
            printf("error\n");
            break;
        }
        if(current_scene == SCENE_EXIT)
            break;

    }

        /*current_scene = login_screen(id, pw);

        int red_score = 3;
        int blue_score = 9;
        Result results = RESULT_WIN;
        
        current_scene = result_function(red_score, blue_score, results);*/

        /*strcpy(nickname, "zxdf"); 
        ReportResult report_result = send_report_request(nickname);
        /*switch (report_result) {
            case REPORT_SUCCESS:
                printf("신고가 완료되었습니다.\n");
                break;
            case REPORT_SUSPENDED:
                printf("신고가 완료되었으며, 해당 사용자는 정지되었습니다.\n");
                break;
            case REPORT_INVALID_TOKEN:
                printf("유효하지 않은 토큰입니다.\n");
                break;
            case REPORT_USER_NOT_FOUND:
                printf("사용자를 찾을 수 없습니다.\n");
                break;
            case REPORT_ERROR:
                printf("신고 중 오류가 발생했습니다.\n");
                break;
        }*/
        /*report_result = send_report_request(nickname);
        report_result = send_report_request(nickname);

        current_scene = login_screen(id, pw);*/
        /*
        while (current_scene != SCENE_MAIN) {
            if (current_scene == SCENE_LOGIN) {
                current_scene = login_screen(id, pw);
            } else if (current_scene == SCENE_SIGNUP) {
                current_scene = signup_screen(id, pw, nickname);
            }
        }

        // 메인 화면(게임) 진입
        if(current_scene == SCENE_MAIN)
            current_scene = lobby_screen();
        if(current_scene == SCENE_INVALID_TOKEN) {
            show_invalid_token_screen();
            current_scene = login_screen(id, pw);
            if (current_scene == SCENE_SIGNUP) {
                current_scene = signup_screen(id, pw, nickname);
            }
        }
        if(current_scene == SCENE_ERROR) {
            printf("error\n");
            break;
        }
        if(current_scene == SCENE_EXIT)
            break;

        // int red_score = 3;
        // int blue_score = 9;
        Result result = RESULT_WIN;
        
        current_scene = result_function(red_score, blue_score, result);
        if(current_scene == SCENE_INVALID_TOKEN) {
            // 경고창 출력 -> 로그인 화면으로 
            show_invalid_token_screen();
            current_scene = login_screen(id, pw);

        }
        if(current_scene == SCENE_ERROR) {
            printf("error\n");
            // break;
        }

        // 
        red_score = 3;
        blue_score = 9;
        // Result results = RESULT_WIN;
        
        current_scene = result_function(red_score, blue_score, results);
        if(current_scene == SCENE_INVALID_TOKEN) {
            // 경고창 출력 -> 로그인 화면으로 */
            /*show_invalid_token_screen();
        }
        if(current_scene == SCENE_ERROR) {
            printf("error\n");
            // break;
        }*/

    /*    
    }

    // (임시) 게임 결과 화면 호출
    // 이것도 while 문 안에 넣어야 함.
    int red_score = 3;
    int blue_score = 9;
    Result result = RESULT_WIN;
    
    current_scene = result_function(red_score, blue_score, result);
    if(current_scene == SCENE_INVALID_TOKEN) {
        // 경고창 출력 -> 로그인 화면으로 */
        /*show_invalid_token_screen();
    }
    if(current_scene == SCENE_ERROR) {
        printf("error\n");
        // break;
    }

    // 
    strcpy(nickname, "aaaa"); 
    ReportResult report_result = send_report_request(nickname);
    switch (report_result) {
        case REPORT_SUCCESS:
            printf("신고가 완료되었습니다.\n");
            break;
        case REPORT_SUSPENDED:
            printf("신고가 완료되었으며, 해당 사용자는 정지되었습니다.\n");
            break;
        case REPORT_INVALID_TOKEN:
            printf("유효하지 않은 토큰입니다.\n");
            break;
        case REPORT_USER_NOT_FOUND:
            printf("사용자를 찾을 수 없습니다.\n");
            break;
        case REPORT_ERROR:
            printf("신고 중 오류가 발생했습니다.\n");
            break;
    }*/


    // 로그인/회원가입 후 SSL 소켓 닫기
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    close(game_sock);
    return 0;
}
