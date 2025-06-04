#include "lobby_screen.h"
#include "gui_util.h"
#include "connect_to_server.h"
#include "waiting_screen.h"
#include "codenames_screen.h"
#include <ncurses.h>
#include <locale.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int current_selection = 0;

void draw_lobby_ui(const UserInfo* user) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    clear();
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

void lobby_screen(const UserInfo* user) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    signal(SIGWINCH, sigwinch_handler_lobby);

    draw_lobby_ui(user);

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
                    return;
                }
                break;
        }

        draw_lobby_ui(user);
    }

    endwin();
}