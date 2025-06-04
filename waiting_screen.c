#include "waiting_screen.h"
#include "gui_util.h"
#include <ncurses.h>
#include <unistd.h>
#include <string.h>

static int progress = 0;
static char current_msg[256] = "Loading...";
static const int bar_length = 30;

// 현재 진행률 메시지 설정
void set_waiting_message(const char* new_msg) {
    strncpy(current_msg, new_msg, sizeof(current_msg) - 1);
    current_msg[sizeof(current_msg) - 1] = '\0';
}

// 내부 콜백 보관
static bool (*progress_task)(int* progress_out) = NULL;

// 로딩 화면 출력
static void draw_screen() {
    clear();

    int y_center = LINES / 2;
    int x_center = COLS / 2;

    mvprintw(y_center - 2, x_center - strlen(current_msg) / 2, "%s", current_msg);

    int bar_start = x_center - (bar_length / 2 + 1);
    mvprintw(y_center, bar_start, "[");

    int filled = (progress * bar_length) / 100;
    for (int i = 0; i < bar_length; ++i) {
        addch(i < filled ? '#' : '-');
    }
    printw("]");

    const char* cancel_msg = "Press Q to cancel";
    mvprintw(y_center + 2, x_center - strlen(cancel_msg) / 2, "%s", cancel_msg);

    draw_border();
    refresh();
}

// 대기 화면 실행 (콜백 기반)
WaitingResult waiting(bool (*task_callback)(int* progress_out)) {
    progress = 0;
    progress_task = task_callback;

    initscr(); 
    noecho(); 
    curs_set(FALSE);
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    WaitingResult result = WAIT_SUCCESS;

    while (1) {
        draw_screen();

        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            result = WAIT_CANCELED;
            break;
        }

        if (progress_task) {
            bool done = progress_task(&progress);
            if (done || progress >= 100) {
                result = WAIT_SUCCESS;
                break;
            }
        }

        usleep(500000);  // 0.5초 간격
    }

    endwin();
    progress = 0;
    return result;
}
