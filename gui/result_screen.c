#include <ncurses.h>
#include <string.h>

typedef enum {
    RESULT_WIN,
    RESULT_LOSE,
} Result;


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

    // 승패 유무 & 결과 값 사이 선
    int y = max_y / 4;
    for (int i = 1; i < max_x -1; i++) {
        mvaddch(y, i, '-');
    }
}

// 결과 그리기
void draw_result(int red_score, int blue_score, Result result) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);


    const char* msg;
    switch (result) {
        case RESULT_WIN: msg = "Y O U   W I N !"; break;
        case RESULT_LOSE: msg = "Y O U   L O S E !"; break;
        default: msg = ""; break;
    }

    int y1 = max_y / 8;
    int x1 = (max_x - strlen(msg)) / 2;
    attron(COLOR_PAIR(1)); // 노란색
    mvprintw(y1, x1, "%s", msg);
    attroff(COLOR_PAIR(1));

    char red_str[32];
    char blue_str[32];
    snprintf(red_str, sizeof(red_str), "RED TEAM : %d", red_score);
    snprintf(blue_str, sizeof(blue_str), "BLUE TEAM : %d", blue_score);

    int y2 = max_y / 2;
    int x2 = (max_x - strlen(red_str)) / 2;

    attron(COLOR_PAIR(2)); // 빨간색
    mvprintw(y2, x2, "%s", red_str);
    attroff(COLOR_PAIR(2));
    attron(COLOR_PAIR(3)); // 파란색
    mvprintw(y2+1, x2, "%s", blue_str);
    attroff(COLOR_PAIR(3));

    char msg4[] = "Enter";
    int y3 = max_y / 4 * 3;
    int x3 = (max_x - strlen(msg4)) / 2;
    mvprintw(y3, x3, "%s", msg4);
}

// 결과 전체
void result_function(int red_score, int blue_score, Result result) {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color(); // 색상 기능 활성화

    // 색상 쌍 설정
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_BLUE, COLOR_BLACK);

    while (1) {
        clear();
        draw_border();
        draw_result(red_score, blue_score, result);
        refresh();

        int ch = getch();
        if (ch == 'q' || ch == 10 || ch == 13) break; // q 혹은 엔터 치면 꺼짐
    }

    endwin();
}

int main() {

    int red_score = 3;
    int blue_score = 9;
    Result result = RESULT_WIN;
    result_function(red_score, blue_score, result);
    
    return 0;
}
