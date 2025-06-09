
#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <stdbool.h>

#define BOARD_SIZE 5
#define LEFT_PANEL_WIDTH 35

typedef enum {
    COMMAND,
    INPUT_HINT,
    INPUT_COUNT,
    INPUT_ANSWER
} InputMode;

const char* words[25] = {
    "고양이", "사과", "기차", "학교", "도서관",
    "의사", "컴퓨터", "바다", "비행기", "음악",
    "산", "강아지", "자동차", "영화", "전화기",
    "축구", "달", "태양", "나무", "빵",
    "물고기", "침대", "시계", "의자", "눈"
};

int teams[25] = {
    0, 1, 2, 1, 2,
    0, 0, 2, 1, 0,
    2, 0, 1, 0, 2,
    1, 2, 1, 0, 0,
    2, 3, 1, 2, 0
};

bool revealed[25] = { false };
int viewer_mode = 0;
char hint_word[32] = "";
char link_count[4] = "";
char answer_text[32] = "";

void draw_card(int y, int x, const char* word, int team, bool is_revealed) {
    int color_pair;
    if (is_revealed || viewer_mode == 1) {
        switch (team) {
            case 1: color_pair = 2; break;
            case 2: color_pair = 3; break;
            case 3: color_pair = 4; break;
            default: color_pair = 1;
        }
    } else {
        color_pair = 1;
    }

    attron(COLOR_PAIR(color_pair));
    mvprintw(y,     x, "+--------+");
    mvprintw(y + 1, x, "|        |");
    int padding = (8 - strlen(word)) / 2;
    mvprintw(y + 1, x + 1 + padding, "%s", word);
    mvprintw(y + 2, x, "+--------+");
    attroff(COLOR_PAIR(color_pair));
}

void draw_board(int offset_x) {
    int start_y = 2;
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            int index = y * BOARD_SIZE + x;
            int draw_y = start_y + y * 4;
            int draw_x = offset_x + x * 12;
            draw_card(draw_y, draw_x, words[index], teams[index], revealed[index]);
        }
    }
}

void draw_team_ui(int y, int x) {
    attron(COLOR_PAIR(2));
    mvprintw(y, x, "빨강 팀      남은 단어 - 8");
    attroff(COLOR_PAIR(2));
    mvprintw(y + 1, x + 2, "bowons     신고");
    mvprintw(y + 2, x + 2, "suwon      신고");
    mvprintw(y + 3, x + 2, "eeee       신고");

    attron(COLOR_PAIR(3));
    mvprintw(y + 5, x, "파랑 팀      남은 단어 - 7");
    attroff(COLOR_PAIR(3));
    mvprintw(y + 6, x + 2, "bowons     신고");
    mvprintw(y + 7, x + 2, "suwon      신고");
    mvprintw(y + 8, x + 2, "eeee       신고");
}

void draw_chat_ui(int y, int x, int height, int width) {
    mvaddch(y, x, ACS_ULCORNER);
    for (int i = 0; i < width - 2; ++i) addch(ACS_HLINE);
    addch(ACS_URCORNER);
    for (int i = 1; i < height - 1; ++i) {
        mvaddch(y + i, x, ACS_VLINE);
        mvaddch(y + i, x + width - 1, ACS_VLINE);
    }
    mvaddch(y + height - 1, x, ACS_LLCORNER);
    for (int i = 0; i < width - 2; ++i) addch(ACS_HLINE);
    addch(ACS_LRCORNER);

    mvprintw(y + 1, x + 2, "bowons: 뭐하냐");
    mvprintw(y + 2, x + 2, "빨간팀 - suwon: 개 아님?");
    mvprintw(y + 3, x + 2, "bowons: 야");

    mvprintw(y + 1, x + width - 2, "↑");
    mvprintw(y + 2, x + width - 2, "█");
    mvprintw(y + 3, x + width - 2, "↓");

    mvaddch(y + height, x, ACS_ULCORNER);
    for (int i = 0; i < width - 12; ++i) addch(ACS_HLINE);
    addch(ACS_TTEE);
    for (int i = 0; i < 9; ++i) addch(ACS_HLINE);
    addch(ACS_URCORNER);

    mvprintw(y + height + 1, x + 1, "반칙아님?");
    mvprintw(y + height + 1, x + width - 10, "[제출]");

    mvaddch(y + height + 2, x, ACS_LLCORNER);
    for (int i = 0; i < width - 2; ++i) addch(ACS_HLINE);
    addch(ACS_LRCORNER);
}

int main() {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();

    init_pair(1, COLOR_BLACK, COLOR_WHITE);       // 중립
    init_pair(2, COLOR_WHITE, COLOR_RED);         // 빨강 카드
    init_pair(3, COLOR_WHITE, COLOR_BLUE);        // 파랑 카드
    init_pair(4, COLOR_WHITE, COLOR_MAGENTA);     // 암살자 카드

    InputMode mode = COMMAND;
    int board_offset_x = LEFT_PANEL_WIDTH + 2;

    while (1) {
        clear();
        mvprintw(0, 2, "코드네임즈 - t: 팀장 전환, i: 입력 모드, q: 종료   [모드: %s]",
            mode == COMMAND ? "명령" :
            mode == INPUT_HINT ? "힌트 입력" :
            mode == INPUT_COUNT ? "연결 수 입력" : "정답 입력");

        draw_team_ui(2, 2);
        draw_chat_ui(2, board_offset_x + 62, 14, 30);
        draw_board(board_offset_x);

        int output_y = 2 + BOARD_SIZE * 4;
        mvprintw(output_y, board_offset_x, "💡 힌트: %s / 연결 수: %s", hint_word, link_count);

        if (viewer_mode == 1) {
            mvprintw(output_y + 1, board_offset_x, "🧠 팀장 모드 - 카드 배치를 확인하세요.");
        } else {
            mvprintw(output_y + 1, board_offset_x, "🙋 팀원 모드 - 정답을 입력해 주세요.");
            mvprintw(output_y + 2, board_offset_x, "정답: %s", answer_text);
        }

        refresh();

        if (mode == INPUT_HINT) {
            echo();
            mvprintw(output_y + 2, board_offset_x, "힌트: ");
            getnstr(hint_word, sizeof(hint_word) - 1);
            noecho();
            mode = INPUT_COUNT;
        } else if (mode == INPUT_COUNT) {
            echo();
            mvprintw(output_y + 3, board_offset_x, "연결 수: ");
            getnstr(link_count, sizeof(link_count) - 1);
            noecho();
            mode = COMMAND;
        } else if (mode == INPUT_ANSWER) {
            echo();
            mvprintw(output_y + 2, board_offset_x, "정답: ");
            getnstr(answer_text, sizeof(answer_text) - 1);
            noecho();
            mode = COMMAND;
        } else {
            int ch = getch();
            if (ch == 'q') break;
            else if (ch == 't') viewer_mode = !viewer_mode;
            else if (ch == 'i') mode = (viewer_mode == 1) ? INPUT_HINT : INPUT_ANSWER;
        }
    }

    endwin();
    return 0;
}
