#include "gui_util.h"
#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

// 전역 상태
static int progress = 0;
static char current_msg[256] = "Loading...";
static const int bar_length = 30;

void set_waiting_message(const char* new_msg) {
    strncpy(current_msg, new_msg, sizeof(current_msg) - 1);
    current_msg[sizeof(current_msg) - 1] = '\0';
}

// 전역 함수 포인터 콜백
static bool (*progress_task)(int* progress_out) = NULL;

// 화면 출력
void draw_screen() {
    clear();

     mvprintw(LINES / 2 - 2, (COLS - strlen(current_msg)) / 2, "%s", current_msg);

    int bar_start = (COLS - (bar_length + 2)) / 2;
    mvprintw(LINES / 2, bar_start, "[");

    int filled = (progress * bar_length) / 100;
    for (int i = 0; i < bar_length; ++i) {
        addch(i < filled ? '#' : '-');
    }
    printw("]");

    const char* cancel = "Press Q to cancel";
    mvprintw(LINES / 2 + 2, (COLS - strlen(cancel)) / 2, "%s", cancel);

    draw_border();
    refresh();
}

// 메인 waiting 실행 함수
void waiting(bool (*task_callback)(int* progress_out)) {
    progress = 0;
    progress_task = task_callback;

    initscr(); noecho(); curs_set(FALSE);
    nodelay(stdscr, TRUE); keypad(stdscr, TRUE);

    while (1) {
        draw_screen();

        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        if (progress_task) {
            bool done = progress_task(&progress);
            if (done || progress >= 100) break;
        }

        usleep(500000);
    }

    endwin();
    progress = 0;
}