#include <ncurses.h>
#include <unistd.h>
#include <string.h>

#define TOTAL_PLAYERS 20

// 테두리 그리기
void draw_border() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // 상단, 하단 테두리
    for (int i = 0; i < max_x; i++) {
        mvaddch(0, i, '-');
        mvaddch(max_y-1, i, '-');
    }
    // 좌측, 우측 테두리
    for (int i = 1; i < max_y-1; i++) {
        mvaddch(i, 0, '|');
        mvaddch(i, max_x-1, '|');
    }
    // 네 귀퉁이
    mvaddch(0, 0, '+');
    mvaddch(0, max_x-1, '+');
    mvaddch(max_y-1, 0, '+');
    mvaddch(max_y-1, max_x-1, '+');

}

// 화면 그리기
void draw_screen(int progress) {
    clear();

    // 1. 상단 텍스트 (중앙)
    const char *msg = "Loading...";
    mvprintw(LINES / 2 - 2, (COLS - strlen(msg)) / 2, "%s", msg);

    // 2. 진행 바 (중앙)
    int bar_width = TOTAL_PLAYERS + 2; // [와 ] 포함
    int bar_start = (COLS - bar_width) / 2;
    mvprintw(LINES / 2, bar_start, "[");
    for (int i = 0; i < TOTAL_PLAYERS; ++i) {
        if (i < progress) {
            addch('#');
        } else {
            addch('-');
        }
    }
    printw("]");

    // 3. Cancel 버튼 (중앙)
    const char *cancel = "Cancel";
    mvprintw(LINES / 2 + 2, (COLS - strlen(cancel)) / 2, "%s", cancel);

    refresh();
}

// 전체
void waiting() {
    initscr();
    noecho();
    curs_set(FALSE);
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    int progress = 0;
    int ch;

    while (1) {
        draw_screen(progress);
        draw_border();

        ch = getch(); // q 누르면 나감
        if (ch == 'q' || ch == 'Q') {
            break;
        }

        // 진행 바 자동 증가 (1초에 1칸, 원하면 usleep 값 조정)
        if (progress <= TOTAL_PLAYERS) {
            progress++;
            usleep(500000); // 1초 대기
        } else if (progress > TOTAL_PLAYERS) {
            progress = 0;
        }
    }
    endwin();
}

int main() {
    
    waiting();
    return 0;
}
