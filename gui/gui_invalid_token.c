#include "gui_invalid_token.h"
#include <ncurses.h>
#include <string.h>
#include <unistd.h>

void show_invalid_token_screen() {
    initscr();
    noecho();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    erase();

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // 경고창 크기
    int win_height = 7;
    int win_width = 40;
    int win_y = (max_y - win_height) / 2;
    int win_x = (max_x - win_width) / 2;

    // 경고창 테두리
    for (int y = 0; y < win_height; ++y) {
        for (int x = 0; x < win_width; ++x) {
            if (y == 0 || y == win_height-1) {
                mvaddch(win_y + y, win_x + x, '-');
            } else if (x == 0 || x == win_width-1) {
                mvaddch(win_y + y, win_x + x, '|');
            }
        }
    }
    // 네 귀퉁이
    mvaddch(win_y, win_x, '+');
    mvaddch(win_y, win_x + win_width-1, '+');
    mvaddch(win_y + win_height-1, win_x, '+');
    mvaddch(win_y + win_height-1, win_x + win_width-1, '+');

    // 메시지 출력
    const char *msg = "Token is invalid or expired";
    mvprintw(win_y + win_height/2 - 1, win_x + (win_width - strlen(msg))/2, "%s", msg);
    const char *press_key = "Press any key to continue...";
    mvprintw(win_y + win_height/2 + 1, win_x + (win_width - strlen(press_key))/2, "%s", press_key);

    refresh();
    getch(); // 아무 키나 입력 대기

    endwin();
}
/*
int main() {
    show_invalid_token_screen();
    return 0;
}*/
