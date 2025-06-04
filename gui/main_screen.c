#include "client.h"
#include "gui_utils.h"
#include "main_screen.h"
#include <ncurses.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

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

void draw_main_screen(UserInfo *info) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int total = info->wins + info->losses;
    double win_rate = (total > 0) ? (info->wins * 100.0 / total) : 0.0;

    char nick_str[64];
    char wins_str[64];
    char losses_str[64];
    char rate_str[64];

    snprintf(nick_str, sizeof(nick_str), "Nickname: %s", info->nickname);
    snprintf(wins_str, sizeof(wins_str), "Win: %d", info->wins);
    snprintf(losses_str, sizeof(losses_str), "Lose: %d", info->losses);
    snprintf(rate_str, sizeof(rate_str), "winning rate: %.1f%%", win_rate);

    int y = max_y / 3;
    int x = (max_x - strlen(nick_str)) / 2;

    attron(A_BOLD);
    mvprintw(y, x, "%s", nick_str);
    mvprintw(y+2, x, "%s", wins_str);
    mvprintw(y+4, x, "%s", losses_str);
    mvprintw(y+6, x, "%s", rate_str);
    attroff(A_BOLD);

    char msg[] = "Press Enter to exit.";
    mvprintw(max_y - 2, (max_x - strlen(msg)) / 2, "%s", msg);
}

SceneState main_screen_ui(void) {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    attron(COLOR_PAIR(1));

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

    while (1) {
        clear();
        draw_border();
        draw_main_screen(&info);
        refresh();

        int ch = getch();
        if (ch == 10 || ch == 13 || ch == 'q') {
            return SCENE_EXIT;
            break;
        }
    }

    endwin();
}
