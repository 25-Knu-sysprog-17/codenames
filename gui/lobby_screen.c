#include "client.h"
#include "lobby_screen.h"
#include "gui_utils.h"
#include "connect_to_server.h"
#include "waiting_screen.h"
#include "codenames_screen.h"
#include <ncurses.h>
#include <locale.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static int current_selection = 0;

// 서버로부터 받은 정보 파싱 (NICKNAME|닉네임|WINS|10|LOSSES|5)
int parse_user_info(const char *response, UserInfo *info) {
    char *p = strchr(response, '|');
    if (!p) return -1;
    p++;

    // 닉네임
    char *nick = p;
    p = strchr(nick, '|');
    if (!p) return -1;
    *p = '\0';
    strncpy(info->nickname, nick, sizeof(info->nickname)-1);
    info->nickname[sizeof(info->nickname)-1] = '\0';
    p++;

    // WINS
    if (strncmp(p, "WINS|", 5) != 0) return -1;
    p += 5;
    info->wins = atoi(p);
    p = strchr(p, '|');
    if (!p) return -1;
    p++;

    // LOSSES
    if (strncmp(p, "LOSSES|", 7) != 0) return -1;
    p += 7;
    info->losses = atoi(p);

    return 0;
}

void draw_lobby_ui(const UserInfo* user) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    erase();
    draw_border();

    int art_start_y = 2;
    int art_start_x = (max_x - strlen(art[0])) / 2;
    for (int i = 0; i < art_lines; i++) {
        mvprintw(art_start_y + i, art_start_x, "%s", art[i]);
    }

    int info_y = art_start_y + art_lines + 2;

    char nickname_line[64];
    snprintf(nickname_line, sizeof(nickname_line), "닉네임: %s", user->nickname);
    mvprintw(info_y, (max_x - strlen(nickname_line)) / 2, "%s", nickname_line);

    char record_line[64];
    snprintf(record_line, sizeof(record_line), "전적: 승 %d / 패 %d", user->wins, user->losses);
    mvprintw(info_y + 2, (max_x - strlen(record_line)) / 2, "%s", record_line);

    double rate = (user->wins + user->losses) > 0
                ? (double)user->wins / (user->wins + user->losses) * 100.0
                : 0.0;

    char rate_line[64];
    snprintf(rate_line, sizeof(rate_line), "승률: %.2f%%", rate);
    mvprintw(info_y + 4, (max_x - strlen(rate_line)) / 2, "%s", rate_line);

    const char* match_button = "[ 랜덤 매칭 시작 ]";
    if (current_selection == 0) attron(A_REVERSE);
    mvprintw(info_y + 7, (max_x - strlen(match_button)) / 2, "%s", match_button);
    if (current_selection == 0) attroff(A_REVERSE);

    refresh();
}

void sigwinch_handler_lobby(int signo) {
    endwin();
    refresh();
    clear();
}

SceneState lobby_screen(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    signal(SIGWINCH, sigwinch_handler_lobby);

    // 토큰을 서버에 전송 (예: "TOKEN|토큰값")
    char token_msg[TOKEN_LEN + 16];
    snprintf(token_msg, sizeof(token_msg), "TOKEN|%s", token);
    send(game_sock, token_msg, strlen(token_msg), 0);

    // 서버로부터 닉네임, 승/패 횟수 받기
    char buffer[256];
    int len = recv(game_sock, buffer, sizeof(buffer) - 1, 0);
    if (len <= 0) {
        perror("recv");
        close(game_sock);
        return SCENE_ERROR;
    }
    buffer[len] = '\0';
    printf("서버 응답: [%s]\n", buffer);

    // 유효하지 않은 토큰인 경우
    if (strcmp(buffer, "INVALID_TOKEN") == 0) {
    // fprintf(stderr, "유효하지 않은 토큰입니다.\n");
        close(game_sock);
        return SCENE_INVALID_TOKEN;
    }
    // 에러
    if (strcmp(buffer, "ERROR") == 0) {
        close(game_sock);
        return SCENE_ERROR;
    }

    // 서버 응답 파싱 (NICKNAME|닉네임|WINS|10|LOSSES|5)
    UserInfo info;
    if (parse_user_info(buffer, &info) < 0) {
        fprintf(stderr, "서버 응답 파싱 실패\n");
        close(game_sock);
        return SCENE_ERROR;
    }

    draw_lobby_ui(&info);

    while (1) {
        int ch = getch();
        if (ch == 'q') break;

        switch (ch) {
            case KEY_UP:
            case KEY_DOWN:
                current_selection = 0;
                break;
            case 10: // Enter
                if (current_selection == 0) {
                    endwin();
                    WaitingResult result = waiting(wait_for_match);
                    if (result == WAIT_SUCCESS) {
                        codenames_screen();  // ✅ system 호출 제거
                    } else {
                        printf("⛔ 매칭이 취소되었습니다.\n");
                    }
                    // return;
                }
                break;
        }

        draw_lobby_ui(&info);
    }

    endwin();
    return SCENE_MAIN;
}