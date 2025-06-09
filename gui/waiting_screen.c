#include "waiting_screen.h"
#include "gui_utils.h"
#include <ncurses.h>
#include <unistd.h>
#include <string.h>

static bool cancel_enabled = true;
static char current_msg[256] = "Loading...";
static const int bar_length = 30;

/// @brief 내부 진행률 표시용 변수 (draw_screen에서만 사용)
static int progress_display = 0;

/// @brief 로딩 화면 그리기 함수
static void draw_screen() {
    clear();

    int y_center = LINES / 2;
    int x_center = COLS / 2;

    mvprintw(y_center - 2, x_center - strlen(current_msg) / 2, "%s", current_msg);

    int bar_start = x_center - (bar_length / 2 + 1);
    mvprintw(y_center, bar_start, "[");

    int filled = (progress_display * bar_length) / 100;
    for (int i = 0; i < bar_length; ++i) {
        addch(i < filled ? '#' : '-');
    }
    printw("]");

    if (cancel_enabled) {
        const char* cancel_msg = "Press Q to cancel";
        mvprintw(y_center + 2, x_center - strlen(cancel_msg) / 2, "%s", cancel_msg);
    }

    draw_border();
    refresh();
}

void set_cancel_enabled(bool enabled) {
    cancel_enabled = enabled;
}

void set_waiting_message(const char* new_msg) {
    strncpy(current_msg, new_msg, sizeof(current_msg) - 1);
    current_msg[sizeof(current_msg) - 1] = '\0';
}

void update_task_status(TaskStatus* status, int progress, bool done, bool success) {
    pthread_mutex_lock(&status->lock);
    status->progress = progress;
    status->done = done;
    status->success = success;
    pthread_mutex_unlock(&status->lock);
}

WaitingResult waiting(TaskStatus* status, void* (*thread_func)(void*)) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, thread_func, status) != 0) {
        return WAIT_FAILED;
    }

    initscr();
    noecho();
    curs_set(FALSE);
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    WaitingResult result = WAIT_SUCCESS;

    while (1) {
        // 사용자 입력 처리
        int ch = getch();
        if ((ch == 'q' || ch == 'Q') && cancel_enabled) {
            result = WAIT_CANCELED;
            break;
        }

        // 상태 읽기
        int prog;
        bool done, success;
        pthread_mutex_lock(&status->lock);
        prog = status->progress;
        done = status->done;
        success = status->success;
        pthread_mutex_unlock(&status->lock);

        progress_display = prog;
        draw_screen();

        // 종료 조건 판별
        if (prog >= 100) {
            if (done)
                result = success ? WAIT_SUCCESS : WAIT_FAILED;
            else
                result = WAIT_CANCELED;
            break;
        }

        usleep(500000); // 0.5초 주기
    }

    pthread_cancel(tid);  // 작업 강제 종료 (필요 시)
    pthread_join(tid, NULL);
    //endwin();
    return result;
}
