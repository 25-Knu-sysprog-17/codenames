// 네트워크 기반으로 리팩토링된 codenames_screen.c
#define _XOPEN_SOURCE 700

#include "client.h"
#include "codenames_screen.h"
#include <wchar.h>
#include <wctype.h>
#include <ncursesw/ncurses.h>
#include <locale.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#define BOARD_SIZE 5
#define LEFT_PANEL_WIDTH 35
#define MAX_CARDS 25
#define CHAT_LOG_SIZE 100
#define MAX_INPUT_LEN 100

typedef struct {
    char name[32];
    int type;
    int isUsed;
} GameCard;

static GameCard cards[MAX_CARDS];
static int redScore = 0;
static int blueScore = 0;
static int turn_team = 0;
static int phase = 0;
static int gameOver = 0;
static char hintWord[32] = "";
static int hintCount = 0;
static int remainingTries = 0;

static char answer_text[32] = "";
static char chat_log[CHAT_LOG_SIZE][128] = { "" };
static int chat_count = 0;
static int chat_scroll_offset = 0;
static int report_selected_index = 0;
static int last_input_stage = 0;

static pthread_mutex_t chat_log_lock = PTHREAD_MUTEX_INITIALIZER;

typedef enum { NONE, INPUT_HINT, INPUT_LINK, INPUT_ANSWER, INPUT_CHAT, INPUT_REPORT } InputMode;
static InputMode input_mode = NONE;

static GameInitInfo global_info;

typedef struct {
    pthread_mutex_t lock;
    wchar_t input_buf[MAX_INPUT_LEN];
    int ready;
    InputMode mode;
} SharedInput;

static SharedInput shared_input = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .input_buf = { 0 },
    .ready = 0,
    .mode = NONE
};

void* input_thread_func(void* arg) {
    wint_t wch;
    wchar_t local_buf[MAX_INPUT_LEN];
    int len = 0;

    while (1) {
        if (get_wch(&wch) == ERR) continue;

        pthread_mutex_lock(&shared_input.lock);

        if (wch == L'\n') {
            shared_input.input_buf[len] = L'\0';
            shared_input.ready = 1;
            len = 0;
        } else if (wch == 127 || wch == KEY_BACKSPACE) {
            if (len > 0) len--;
        } else if (wch == L'\t') {  // Tab → 채팅 모드 전환
            shared_input.mode = INPUT_CHAT;
            shared_input.ready = 1;
            len = 0;
        } else if (wch == 1) {     // Ctrl+A → 신고 모드
            shared_input.mode = INPUT_REPORT;
            shared_input.ready = 1;
            len = 0;
        } else if (iswprint(wch) && len < MAX_INPUT_LEN - 1) {
            local_buf[len++] = wch;
        }

        wcsncpy(shared_input.input_buf, local_buf, MAX_INPUT_LEN);
        pthread_mutex_unlock(&shared_input.lock);
    }
    return NULL;
}

void draw_card(int y, int x, const char* word, int team, bool is_revealed, int is_Used) {
    int color_pair = is_revealed ? (team == 1 ? 2 : team == 2 ? 3 : team == 3 ? 4 : 1) : 1;
    attron(COLOR_PAIR(color_pair));
    mvprintw(y,     x, "+--------+");
    mvprintw(y + 1, x, "|        |");
    int padding = (8 - strlen(word)) / 2;
    if (!is_Used)
        mvprintw(y + 1, x + 1 + padding, "%s", word);
    else
        mvprintw(y + 1, x + 1 + padding, "완료");
    mvprintw(y + 2, x, "+--------+");
    attroff(COLOR_PAIR(color_pair));
}

void draw_board(int offset_x, int offset_y, bool reveal_all) {
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            int index = y * BOARD_SIZE + x;
            int draw_y = offset_y + y * 4;
            int draw_x = offset_x + x * 12;
            bool is_revealed = reveal_all || cards[index].isUsed;
            draw_card(draw_y, draw_x, cards[index].name, cards[index].type, is_revealed, cards[index].isUsed);
        }
    }
}

void draw_team_ui(int y, int x, int red, int blue) {
    attron(COLOR_PAIR(2));
    mvprintw(y, x, "빨강 팀      남은 단어 - %d", 9 - red);
    attroff(COLOR_PAIR(2));

    for (int i = 0; i < 3; i++) {
        if (input_mode == INPUT_REPORT && report_selected_index == i)
            attron(A_REVERSE);
        mvprintw(y + 1 + i, x + 2, "%s     신고", global_info.players[i].nickname);
        if (input_mode == INPUT_REPORT && report_selected_index == i)
            attroff(A_REVERSE);
    }

    attron(COLOR_PAIR(3));
    mvprintw(y + 5, x, "파랑 팀      남은 단어 - %d", 8 - blue);
    attroff(COLOR_PAIR(3));

    for (int i = 3; i < 6; i++) {
        if (input_mode == INPUT_REPORT && report_selected_index == i)
            attron(A_REVERSE);
        mvprintw(y + 1 + i + 2, x + 2, "%s      신고", global_info.players[i].nickname);
        if (input_mode == INPUT_REPORT && report_selected_index == i)
            attroff(A_REVERSE);
    }
}

void draw_chat_ui(int y, int x) {
    y++; 
    mvprintw(y, x, "-------------------<채팅 로그>-------------------");

    pthread_mutex_lock(&chat_log_lock);

    int visible_lines = 10;
    int start = chat_count > visible_lines ? chat_count - visible_lines - chat_scroll_offset : 0;

    for (int i = 0; i < visible_lines; i++) {
        int idx = start + i;
        if (idx < chat_count) {
            const char* msg = chat_log[idx];
            if (msg[0] == '\x01') {
                int color = msg[1] - '0';
                attron(COLOR_PAIR(color));
                mvprintw(y + i + 1, x, "%s", msg + 2); // 내용 출력
                attroff(COLOR_PAIR(color));
            } else {
                mvprintw(y + i + 1, x, "%s", msg);
            }
        } else {
            mvprintw(y + i + 1, x, "                    ");
        }
    }

    pthread_mutex_unlock(&chat_log_lock);

    mvprintw(y + visible_lines - 11, x, "Tab으로 채팅 전환 Ctrl+A로 신고모드 전환 / ↑↓ 스크롤");
}

void redraw_chat_input_line(int y, int x, const wchar_t* chat_input) {
    int width = wcswidth(chat_input, -1);
    if (width < 0) width = 0;
    move(y, x);
    clrtoeol();
    mvaddwstr(y, x, chat_input);
    move(y, x + width);
    refresh();
}

void handle_dynamic_input(char *buffer, size_t size, int y, int x) {
    wchar_t wbuffer[128] = L""; // 유니코드 입력 버퍼
    int len = 0;
    wint_t wch;
    move(y, x);
    curs_set(1); // 커서 보이기

    // 🔁 현재 입력 모드 기억
    if (input_mode == INPUT_HINT) last_input_stage = 0;
    else if (input_mode == INPUT_LINK) last_input_stage = 1;
    else if (input_mode == INPUT_ANSWER) last_input_stage = 2;

    while (1) {
        int result = get_wch(&wch); // 유니코드 문자 입력
        if (result == ERR) continue;

        if (wch == L'\n') break; // Enter 입력 시 종료
        else if (wch == L'\t') { // Tab → 채팅 모드 전환
            input_mode = INPUT_CHAT;
            return;
        } else if (wch == 1) { // Ctrl+A → 신고 모드
            input_mode = INPUT_REPORT;
            return;
        } else if (wch == KEY_BACKSPACE || wch == 127) {
            if (len > 0) {
                len--;
                wbuffer[len] = L'\0';
                redraw_chat_input_line(y, x, wbuffer);
            }
        } else if (iswprint(wch) && len < sizeof(wbuffer) / sizeof(wchar_t) - 1) {
            wbuffer[len++] = wch;
            wbuffer[len] = L'\0';
            redraw_chat_input_line(y, x, wbuffer);

        }
    }

    // 입력 완료 후 변환하여 저장
    wcstombs(buffer, wbuffer, size);
    curs_set(0); // 커서 숨기기
}

void handle_chat_input(int y, int x, char *name, char *raw_msg_out) {
    wchar_t chat_input[MAX_INPUT_LEN] = L""; // 유니코드 입력 버퍼
    mvprintw(y, x, "채팅 입력:");
    refresh();

    int len = 0;
    wint_t wch;
    curs_set(1); // 커서 보이기

    while (1) {
        int result = get_wch(&wch);
        if (result == ERR) continue;
        if (wch == L'\n') break;
        else if (wch == L'\t') {
            if (input_mode == INPUT_CHAT) {
                if (last_input_stage == 0) input_mode = INPUT_HINT;
                else if (last_input_stage == 1) input_mode = INPUT_LINK;
                else if (last_input_stage == 2) input_mode = INPUT_ANSWER;
            } else {
                input_mode = INPUT_CHAT;
            }
            return;
        } else if (wch == 1) {
            input_mode = INPUT_REPORT;
            return;
        } else if (wch == KEY_BACKSPACE || wch == 127) {
            if (len > 0) {
                len--;
                chat_input[len] = L'\0';
                redraw_chat_input_line(y, x + 10, chat_input);
            }
        } else if (wch == KEY_UP) {
            if (chat_scroll_offset + 1 < chat_count)
                chat_scroll_offset++;
            return;
        } else if (wch == KEY_DOWN) {
            if (chat_scroll_offset > 0)
                chat_scroll_offset--;
            return;
        } else if (iswprint(wch) && len < MAX_INPUT_LEN - 1) {
            chat_input[len++] = wch;
            chat_input[len] = L'\0';
            redraw_chat_input_line(y, x + 10, chat_input);
        }
    }

    curs_set(0);

    if (wcslen(chat_input) > 0) {
        char converted[MAX_INPUT_LEN];
        wcstombs(converted, chat_input, sizeof(converted));
        converted[MAX_INPUT_LEN - 1] = '\0';
        strncpy(raw_msg_out, converted, MAX_INPUT_LEN - 1);
        raw_msg_out[MAX_INPUT_LEN - 1] = '\0';
    } else {
        raw_msg_out[0] = '\0';
    }
}


void append_chat_log(const char* msg) {
    pthread_mutex_lock(&chat_log_lock);

    if (chat_count < CHAT_LOG_SIZE) {
        strncpy(chat_log[chat_count++], msg, 127);
        chat_log[chat_count - 1][127] = '\0';  // 안전하게 문자열 종료
    } else {
        for (int i = 1; i < CHAT_LOG_SIZE; i++) {
            strncpy(chat_log[i - 1], chat_log[i], 127);
        }
        strncpy(chat_log[CHAT_LOG_SIZE - 1], msg, 127);
        chat_log[CHAT_LOG_SIZE - 1][127] = '\0';
    }

    chat_scroll_offset = 0;

    pthread_mutex_unlock(&chat_log_lock);
}

// 색상별 채팅 로그 추가
void append_chat_log_colored(const char* msg, int color_pair) {
    char colored_msg[160];
    snprintf(colored_msg, sizeof(colored_msg), "\x01%d\x01%s", color_pair, msg);
    // 앞부분에 색상 정보를 삽입 (\x01 + color + \x01)

    pthread_mutex_lock(&chat_log_lock);

    if (chat_count < CHAT_LOG_SIZE) {
        strncpy(chat_log[chat_count++], colored_msg, 127);
        chat_log[chat_count - 1][127] = '\0';
    } else {
        for (int i = 1; i < CHAT_LOG_SIZE; i++) {
            strncpy(chat_log[i - 1], chat_log[i], 127);
        }
        strncpy(chat_log[CHAT_LOG_SIZE - 1], colored_msg, 127);
        chat_log[CHAT_LOG_SIZE - 1][127] = '\0';
    }

    chat_scroll_offset = 0;

    pthread_mutex_unlock(&chat_log_lock);
}

void* listener_thread(void* arg) {
    int sock = get_game_sock();
    char buffer[1024];
    while (!gameOver) {
        int len = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) break;
        buffer[len] = '\0';

        if (strncmp(buffer, "CARD_UPDATE|", 12) == 0) {
            int idx, used;
            sscanf(buffer + 12, "%d|%d", &idx, &used);
            if (idx >= 0 && idx < MAX_CARDS) cards[idx].isUsed = used;
        } else if (strncmp(buffer, "TURN_UPDATE|", 12) == 0) {
            sscanf(buffer + 12, "%d|%d|%d|%d", &turn_team, &phase, &redScore, &blueScore);
        } else if (strncmp(buffer, "HINT|", 5) == 0) {
            sscanf(buffer + 5, "%d|%31[^|]|%d", &turn_team, hintWord, &hintCount);
            remainingTries = hintCount + 1;
        } else if (strncmp(buffer, "CHAT|", 5) == 0) {
            // 예: "CHAT|0|1|Alex|안녕하세요"
            int sender_team, sender_is_leader;
            char nickname[64], content[128];
            if (sscanf(buffer + 5, "%d|%d|%63[^|]|%127[^\n]", &sender_team, &sender_is_leader, nickname, content) == 4) {
                // 내 정보
                int my_index = global_info.myPlayerIndex;
                int my_team = global_info.players[my_index].team;
                bool my_is_leader = global_info.players[my_index].is_leader;

                bool visible = false;

                if (sender_team == my_team) {
                    // 같은 팀
                    if ((sender_is_leader && my_is_leader) || (!sender_is_leader && !my_is_leader)) {
                        visible = true;
                    }
                } else {
                    // 다른 팀이면 팀장끼리는 볼 수 있음
                    if (sender_is_leader && my_is_leader) {
                        visible = true;
                    }
                }

                if (visible) {
                    char formatted[256];
                    snprintf(formatted, sizeof(formatted), "%s: %s", nickname, content);

                    int color = (sender_team == 0) ? 2 : 3;  // 0 = 레드, 1 = 블루
                    append_chat_log_colored(formatted, color);
                }
            }
        } else if (strncmp(buffer, "GAME_OVER|", 10) == 0) {
            gameOver = 1;
        }
    }
    return NULL;
}

SceneState codenames_screen(GameInitInfo info) {
    global_info = info;

    setlocale(LC_ALL, "");
    initscr(); 
    
    cbreak(); noecho(); keypad(stdscr, TRUE); start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_WHITE, COLOR_RED);
    init_pair(3, COLOR_WHITE, COLOR_BLUE);
    init_pair(4, COLOR_WHITE, COLOR_MAGENTA);

    int sock = get_game_sock();

    send(sock, "GET_ALL_CARDS", strlen("GET_ALL_CARDS"), 0);
    char recv_buf[2048];
    int len = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
    recv_buf[len] = '\0';

    if (strncmp(recv_buf, "ALL_CARDS|", 10) == 0) {
        char* token = strtok(recv_buf + 10, "|");
        for (int i = 0; i < MAX_CARDS && token; i++) {
            strncpy(cards[i].name, token, sizeof(cards[i].name) - 1);
            cards[i].name[sizeof(cards[i].name) - 1] = '\0';
            token = strtok(NULL, "|");
            if (!token) break;
            cards[i].type = atoi(token);
            token = strtok(NULL, "|");
            if (!token) break;
            cards[i].isUsed = atoi(token);
            token = strtok(NULL, "|");
        }
    }

    pthread_t tid;
    pthread_create(&tid, NULL, listener_thread, NULL);
    pthread_detach(tid);

    append_chat_log("[게임 시작!]");
    char init_msg[64];
    snprintf(init_msg, sizeof(init_msg), "[%s팀의 턴입니다.]", turn_team == 0 ? "레드" : "블루");
    append_chat_log(init_msg);

    while (1) {
        erase();
        int term_x, term_y;
        getmaxyx(stdscr, term_y, term_x);
        (void)term_y;

        int board_offset_x = (term_x - (12 * BOARD_SIZE)) / 2;
        int board_offset_y = 2;

        if (gameOver) {
            mvprintw(0, 2, "게임 종료! 빨강팀 %d점, 파랑팀 %d점", redScore, blueScore);
            mvprintw(3, 4, redScore >= 9 ? "레드팀 승리!" : "블루팀 승리!");
            refresh(); getch(); break;
        }

        const char* team = (turn_team == 0) ? "빨강팀" : "파랑팀";
        const char* role = (phase == 0) ? "팀장" : "팀원";
        mvprintw(0, 2, "[%s %s 차례]", team, role);

        draw_team_ui(2, 2, redScore, blueScore);

        bool is_leader = global_info.players[global_info.myPlayerIndex].is_leader;
        draw_board(board_offset_x, board_offset_y, is_leader);

        int chat_y = board_offset_y + BOARD_SIZE * 4 + 2;
        draw_chat_ui(chat_y, 2);
        refresh();

        if (input_mode == INPUT_CHAT) {
            char raw_msg[128] = "";
            handle_chat_input(chat_y + 14, 2, global_info.players[global_info.myPlayerIndex].nickname, raw_msg);

            if (strlen(raw_msg) > 0) {
                char msg[160];
                snprintf(msg, sizeof(msg), "CHAT|%s", raw_msg);
                send(sock, msg, strlen(msg), 0);
            }
            continue;
        }

        if (input_mode == INPUT_REPORT) { //네트워크 처리 미구현
            int ch = getch();
            if (ch == KEY_UP && report_selected_index > 0) report_selected_index--;
            else if (ch == KEY_DOWN && report_selected_index < 5) report_selected_index++;
            else if (ch == 10) {
                char msg[128];
                snprintf(msg, sizeof(msg), "[신고 완료] %s 님을 신고했습니다.", global_info.players[report_selected_index].nickname);
                append_chat_log(msg);
                input_mode = INPUT_CHAT;
            } else if (ch == 9) input_mode = INPUT_CHAT;
            else if (ch == 1) input_mode = INPUT_HINT;
            continue;
        }

        int y = board_offset_y + BOARD_SIZE * 4;

        if (global_info.players[global_info.myPlayerIndex].team == turn_team &&
            global_info.players[global_info.myPlayerIndex].is_leader && phase == 0) {
            mvprintw(y, board_offset_x, "힌트를 입력하세요: ");
            input_mode = INPUT_HINT;
            handle_dynamic_input(hintWord, sizeof(hintWord), y, board_offset_x + 20);
            if (input_mode != INPUT_HINT) continue;

            mvprintw(y + 1, board_offset_x, "연결 수를 입력하세요: ");
            char temp[4] = "";
            input_mode = INPUT_LINK;
            handle_dynamic_input(temp, sizeof(temp), y + 1, board_offset_x + 23);
            if (input_mode != INPUT_LINK || strlen(temp) == 0) continue;

            hintCount = atoi(temp);
            char out[128];
            snprintf(out, sizeof(out), "HINT|%s|%d", hintWord, hintCount);
            send(sock, out, strlen(out), 0);
            continue;
        }

        if (global_info.players[global_info.myPlayerIndex].team == turn_team &&
            !global_info.players[global_info.myPlayerIndex].is_leader && phase == 1) {
            mvprintw(y, board_offset_x, "정답: ");
            input_mode = INPUT_ANSWER;
            handle_dynamic_input(answer_text, sizeof(answer_text), y, board_offset_x + 5);
            if (input_mode != INPUT_ANSWER) continue;

            char msg[64];
            snprintf(msg, sizeof(msg), "ANSWER|%s", answer_text);
            send(sock, msg, strlen(msg), 0);
        }

        input_mode = NONE;
    }

    endwin();
    return SCENE_RESULT;
}
