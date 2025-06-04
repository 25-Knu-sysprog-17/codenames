#include "codenames_screen.h"
#include "connect_to_server.h"
#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#define BOARD_SIZE 5
#define LEFT_PANEL_WIDTH 35
#define CHAT_LOG_SIZE 10

typedef enum {
    FOCUS_HINT,
    FOCUS_LINK,
    FOCUS_ANSWER,
    FOCUS_CHAT
} FocusState;

static const char* words[25] = {
    "고양이", "사과", "기차", "학교", "도서관",
    "의사", "컴퓨터", "바다", "비행기", "음악",
    "산", "강아지", "자동차", "영화", "전화기",
    "축구", "달", "태양", "나무", "빵",
    "물고기", "침대", "시계", "의자", "눈"
};

static int teams[25] = {
    0, 1, 2, 1, 2,
    0, 0, 2, 1, 0,
    2, 0, 1, 0, 2,
    1, 2, 1, 0, 0,
    2, 3, 1, 2, 0
};

static bool revealed[25] = { false };
static int viewer_mode = 0;
static char hint_word[32] = "";
static char link_count[4] = "";
static char answer_text[32] = "";
static char chat_input[128] = "";
static char chat_log[CHAT_LOG_SIZE][128] = { "" };
static int chat_count = 0;

static FocusState focus = FOCUS_HINT;
static bool in_input_mode = false;

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

void draw_chat_log(int y, int x, int height) {
    int start = (chat_count > height) ? chat_count - height : 0;
    for (int i = 0; i < height; i++) {
        if (start + i < chat_count) {
            mvprintw(y + i, x, "%s", chat_log[start + i]);
        }
    }
}

void draw_inputs(int y, int x) {
    if (focus == FOCUS_HINT) attron(A_REVERSE);
    mvprintw(y, x, "💡 힌트: %s", hint_word);
    if (focus == FOCUS_HINT) attroff(A_REVERSE);

    if (focus == FOCUS_LINK) attron(A_REVERSE);
    mvprintw(y + 1, x, "🔗 연결 수: %s", link_count);
    if (focus == FOCUS_LINK) attroff(A_REVERSE);

    if (focus == FOCUS_ANSWER) attron(A_REVERSE);
    mvprintw(y + 2, x, "🙋 정답: %s", answer_text);
    if (focus == FOCUS_ANSWER) attroff(A_REVERSE);

    if (focus == FOCUS_CHAT) attron(A_REVERSE);
    mvprintw(y + 5, x, "💬 채팅: %s", chat_input);
    if (focus == FOCUS_CHAT) attroff(A_REVERSE);

    if (viewer_mode == 1)
        mvprintw(y + 7, x, "🧠 팀장 모드 - 카드 배치를 확인하세요.");
    else
        mvprintw(y + 7, x, "🙋 팀원 모드 - 정답을 입력해 주세요.");
}

void* receive_chat_thread(void* arg) {
    int sock = get_server_socket();
    while (1) {
        char buf[256];
        int bytes = recv(sock, buf, sizeof(buf) - 1, 0);
        if (bytes <= 0) break;

        buf[bytes] = '\0';
        if (strncmp(buf, "CHAT|", 5) == 0) {
            char* name = strtok(buf + 5, "|");
            char* msg = strtok(NULL, "\n");
            if (name && msg) {
                char entry[128];
                snprintf(entry, sizeof(entry), "%s: %s", name, msg);
                if (chat_count < CHAT_LOG_SIZE) {
                    strcpy(chat_log[chat_count++], entry);
                } else {
                    for (int i = 1; i < CHAT_LOG_SIZE; i++) {
                        strcpy(chat_log[i - 1], chat_log[i]);
                    }
                    strcpy(chat_log[CHAT_LOG_SIZE - 1], entry);
                }
            }
        }
    }
    return NULL;
}

void resize_handler(int sig) {
    endwin();
    refresh();
    clear();
    resize_term(0, 0);
}

void codenames_screen() {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    signal(SIGWINCH, resize_handler);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_WHITE, COLOR_RED);
    init_pair(3, COLOR_WHITE, COLOR_BLUE);
    init_pair(4, COLOR_WHITE, COLOR_MAGENTA);

    pthread_t recv_tid;
    pthread_create(&recv_tid, NULL, receive_chat_thread, NULL);
    pthread_detach(recv_tid);

    while (1) {
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        int board_offset_x = (max_x - (12 * BOARD_SIZE)) / 2;

        erase();
        mvprintw(0, 2, "코드네임즈 - ↑↓: 이동, Enter: 입력, t: 팀장모드, q: 종료");

        draw_board(board_offset_x);
        draw_inputs(BOARD_SIZE * 4 + 3, board_offset_x);
        draw_chat_log(2, 2, 10);

        refresh();

        if (in_input_mode) {
            echo();
            curs_set(1);
            if (focus == FOCUS_HINT) {
                move(BOARD_SIZE * 4 + 3, board_offset_x + 10);
                getnstr(hint_word, sizeof(hint_word) - 1);
            } else if (focus == FOCUS_LINK) {
                move(BOARD_SIZE * 4 + 4, board_offset_x + 13);
                getnstr(link_count, sizeof(link_count) - 1);
            } else if (focus == FOCUS_ANSWER) {
                move(BOARD_SIZE * 4 + 5, board_offset_x + 10);
                getnstr(answer_text, sizeof(answer_text) - 1);
            } else if (focus == FOCUS_CHAT) {
                move(BOARD_SIZE * 4 + 8, board_offset_x + 10);
                getnstr(chat_input, sizeof(chat_input) - 1);
                int sock = get_server_socket();
                char sendbuf[256];
                snprintf(sendbuf, sizeof(sendbuf), "CHAT|guest|%s", chat_input); // "guest"로 대체
                send(sock, sendbuf, strlen(sendbuf), 0);
                strcpy(chat_input, "");
            }
            noecho();
            curs_set(0);
            in_input_mode = false;
            continue;
        }

        int ch = getch();
        if (ch == 'q') break;
        else if (ch == 't') viewer_mode = !viewer_mode;
        else if (ch == KEY_UP && focus > 0) focus--;
        else if (ch == KEY_DOWN && focus < FOCUS_CHAT) focus++;
        else if (ch == 10) in_input_mode = true;
    }

    endwin();
}