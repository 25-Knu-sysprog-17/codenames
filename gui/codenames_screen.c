// 네트워크 기반으로 리팩토링된 codenames_screen.c
#define _XOPEN_SOURCE 700

#include "client.h"
#include "codenames_screen.h"
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <ncursesw/ncurses.h>
#include <locale.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>

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

typedef enum {
    REPORT_SUCCESS,
    REPORT_ERROR,
    REPORT_INVALID_TOKEN,
    REPORT_USER_NOT_FOUND,
    REPORT_SUSPENDED
} ReportResult;

static GameCard cards[MAX_CARDS];
int redScore = 0;
int blueScore = 0;
Result result = RESULT_WIN;
int winner_team = -1;
static int turn_team = -1;
static int phase = -1;
static int gameOver = 0;
static int gameLoop = 1;
static char hintWord[32] = "";
static int hintCount = 0;

static char answer_text[32] = "";
static char chat_log[CHAT_LOG_SIZE][128] = { "" };
static int chat_count = 0;
static int chat_scroll_offset = 0;
static int report_selected_index = 0;
static int report_completed = 0;

static pthread_mutex_t chat_log_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int listener_alive = 0;

typedef enum { NONE, INPUT_HINT, INPUT_LINK, INPUT_ANSWER, INPUT_CHAT, INPUT_REPORT } InputMode;

static GameInitInfo global_info;

typedef struct {
    pthread_mutex_t lock;
    wchar_t input_buf[MAX_INPUT_LEN];
    int ready;
    InputMode mode;
    int needs_redraw; // <- 리드로우용
    int cursor_y;
    int cursor_x;
} SharedInput;

static SharedInput shared_input = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .input_buf = { 0 },
    .ready = 0,
    .mode = NONE
};

// 디버그용
#define DEBUG_MSG_COUNT 3
static char debug_messages[DEBUG_MSG_COUNT][1024] = { "" };
static pthread_mutex_t debug_lock = PTHREAD_MUTEX_INITIALIZER;

void set_debug_message(int index, const char* msg) {
    if (index >= 0 && index < DEBUG_MSG_COUNT) {
        pthread_mutex_lock(&debug_lock);
        strncpy(debug_messages[index], msg, sizeof(debug_messages[index]) - 1);
        debug_messages[index][sizeof(debug_messages[index]) - 1] = '\0';
        pthread_mutex_unlock(&debug_lock);
    }
}

void draw_debug_messages(int y, int x) {
    pthread_mutex_lock(&debug_lock);
    for (int i = 0; i < DEBUG_MSG_COUNT; i++) {
        mvprintw(y - DEBUG_MSG_COUNT + i, x, "%s", debug_messages[i]);
    }
    pthread_mutex_unlock(&debug_lock);
}

void listener_signal_handler(int sig) {
    fprintf(stderr, "\n💥 [listener_thread] 크래시 감지됨: 시그널 %d (%s)\n", sig, strsignal(sig));
    fflush(stderr);
    _exit(1);
}


static bool cards_initialized = 0;

ReportResult send_report_request(const char *nickname) {
    char request[256];
    snprintf(request, sizeof(request), "REPORT|%s|%s", token, nickname);

    if (SSL_write(ssl, request, strlen(request)) <= 0) {
        perror("SSL_write");
        return REPORT_ERROR;
    }

    char response[256] = {0};
    int recv_len = SSL_read(ssl, response, sizeof(response)-1);
    if (recv_len <= 0) {
        perror("SSL_read");
        return REPORT_ERROR;
    }
    response[recv_len] = '\0';

    // 응답 파싱
    if (strstr(response, "REPORT_OK")) {
        if (strstr(response, "SUSPENDED")) {
            return REPORT_SUSPENDED;
        }
        return REPORT_SUCCESS;
    } else if (strstr(response, "INVALID_TOKEN")) {
        return REPORT_INVALID_TOKEN;
    } else if (strstr(response, "USER_NOT_FOUND")) {
        return REPORT_USER_NOT_FOUND;
    } else {
        return REPORT_ERROR;
    }
}

bool are_cards_valid() {
    for (int i = 0; i < MAX_CARDS; i++) {
        if (cards[i].name[0] == '\0') return false;
        if (cards[i].type < 0 || cards[i].type > 3) return false;
    }
    return true;
}

void* input_thread_func(void* arg) {
    wint_t wch;
    wchar_t local_buf[MAX_INPUT_LEN] = { 0 };  // ✅ 초기화 한 번만
    int len = 0;

    while (gameLoop) {
        if (get_wch(&wch) == ERR) continue;

        pthread_mutex_lock(&shared_input.lock);

        char dbg[128];
        wcstombs(dbg, local_buf, sizeof(dbg) - 1);
        dbg[sizeof(dbg) - 1] = '\0';

        // ✅ 방향키 처리: 신고 인덱스 선택 또는 채팅 스크롤 / 기타 방향키 무시
        if (wch == KEY_UP || wch == KEY_DOWN || wch == KEY_LEFT || wch == KEY_RIGHT) {
            if (shared_input.mode == INPUT_REPORT) {
                if (wch == KEY_UP && report_selected_index > 0) {
                    report_selected_index--;
                    set_debug_message(1, "신고 인덱스 ↑");
                } else if (wch == KEY_DOWN && report_selected_index < 5) {
                    report_selected_index++;
                    set_debug_message(1, "신고 인덱스 ↓");
                }
                shared_input.needs_redraw = 1;
            }
            else if (shared_input.mode == INPUT_CHAT) {
                pthread_mutex_unlock(&shared_input.lock);  // 🔓 UI 상태 보호 안함

                pthread_mutex_lock(&chat_log_lock);
                if (wch == KEY_UP && chat_scroll_offset + 1 < chat_count)
                    chat_scroll_offset++;
                else if (wch == KEY_DOWN && chat_scroll_offset > 0)
                    chat_scroll_offset--;
                pthread_mutex_unlock(&chat_log_lock);

                pthread_mutex_lock(&shared_input.lock);
                shared_input.needs_redraw = 1;
                pthread_mutex_unlock(&shared_input.lock);

                set_debug_message(1, "↑↓ 스크롤 동작");
                continue;
            }

            // LEFT/RIGHT 등 기타 방향키는 아무 효과 없이 무시되도록 처리
            pthread_mutex_unlock(&shared_input.lock);
            continue;
        }

        if (wch == L'\n') {
            if (len >= MAX_INPUT_LEN) len = MAX_INPUT_LEN - 1;
            local_buf[len] = L'\0';  // 종료 문자 삽입

            wcsncpy(shared_input.input_buf, local_buf, MAX_INPUT_LEN - 1);
            shared_input.input_buf[MAX_INPUT_LEN - 1] = L'\0';  // 명시적 종료

            shared_input.ready = 1;
            shared_input.needs_redraw = 1;

            len = 0;
            
            wmemset(local_buf, 0, MAX_INPUT_LEN);  // ✅ 여기서만 초기화
            pthread_mutex_unlock(&shared_input.lock);
            continue;

        } else if (wch == 127 || wch == KEY_BACKSPACE) {
            if (len > 0) len--;
            local_buf[len] = L'\0';  // 덮어쓰기
            shared_input.needs_redraw = 1;
            set_debug_message(1, "⌫ Backspace 처리됨");            
        } else if (wch == L'\t') {
            int my_index = global_info.myPlayerIndex;
            int my_team = global_info.players[my_index].team;
            bool is_leader = global_info.players[my_index].is_leader;

            if (shared_input.mode == INPUT_CHAT) {
                if (my_team == turn_team && is_leader && phase == 0) {
                    if (strlen(hintWord) > 0) {
                        shared_input.mode = INPUT_LINK;
                        shared_input.needs_redraw = 1;
                        set_debug_message(1, "Tab: 채팅 → LINK (힌트 이미 입력됨)");
                    } else {
                        shared_input.mode = INPUT_HINT;
                        shared_input.needs_redraw = 1;
                        set_debug_message(1, "Tab: 채팅 → HINT");
                    }
                } else if (my_team == turn_team && !is_leader && phase == 1) {
                    shared_input.mode = INPUT_ANSWER;
                    shared_input.needs_redraw = 1;
                    set_debug_message(1, "Tab: 채팅 → ANSWER");
                } else {
                    shared_input.mode = NONE;
                    shared_input.needs_redraw = 1;
                    set_debug_message(1, "Tab: 채팅 → NONE (내 턴 아님)");
                }
            } else {
                shared_input.mode = INPUT_CHAT;
                shared_input.needs_redraw = 1;
                set_debug_message(1, "Tab: 입력모드 → 채팅");
            }

            len = 0;
            wmemset(local_buf, 0, MAX_INPUT_LEN);

        } else if (wch == 1) {
            shared_input.mode = INPUT_REPORT;
            shared_input.needs_redraw = 1;

            len = 0;
            wmemset(local_buf, 0, MAX_INPUT_LEN);
            set_debug_message(1, "Ctrl+A: 입력모드 → 신고");

        } else if (!iswcntrl(wch) && wch < 0x110000 && len < MAX_INPUT_LEN - 1) {
            local_buf[len++] = wch;
            local_buf[len] = L'\0';  // 안전하게 null 종결

            snprintf(dbg, sizeof(dbg), "문자 입력: %lc", wch);
            shared_input.needs_redraw = 1;
            set_debug_message(1, dbg);
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
    mvprintw(y, x, "레드 팀      남은 단어 - %d", 9 - red);
    attroff(COLOR_PAIR(2));

    pthread_mutex_lock(&shared_input.lock);
    InputMode current_mode = shared_input.mode;
    pthread_mutex_unlock(&shared_input.lock);

    for (int i = 0; i < 3; i++) {
        if (current_mode == INPUT_REPORT && report_selected_index == i)
            attron(A_REVERSE);
        mvprintw(y + 1 + i, x + 2, "%s     신고", global_info.players[i].nickname);
        if (current_mode == INPUT_REPORT && report_selected_index == i)
            attroff(A_REVERSE);
    }

    attron(COLOR_PAIR(3));
    mvprintw(y + 5, x, "블루 팀      남은 단어 - %d", 8 - blue);
    attroff(COLOR_PAIR(3));

    for (int i = 3; i < 6; i++) {
        if (current_mode == INPUT_REPORT && report_selected_index == i)
            attron(A_REVERSE);
        mvprintw(y + 1 + i + 2, x + 2, "%s      신고", global_info.players[i].nickname);
        if (current_mode == INPUT_REPORT && report_selected_index == i)
            attroff(A_REVERSE);
    }

    if(report_completed == 1) {
        mvprintw(11 , 4, "[신고 완료] %s 님을 신고했습니다.", global_info.players[report_selected_index].nickname);
        report_completed = 0;
    } else if (report_completed == -1) {
        mvprintw(11 , 4, "신고 중 오류가 발생했습니다.\n");
        report_completed = 0;
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
                char* content = strchr(msg + 2, '\x01');  // 두 번째 \x01 찾기
                if (content) {
                    content++;  // 그 이후부터가 실제 메시지
                    attron(COLOR_PAIR(color));
                    mvprintw(y + i + 1, x, "%s", content);
                    clrtoeol();
                    attroff(COLOR_PAIR(color));
                }
            } else {
                mvprintw(y + i + 1, x, "%s", msg);
            }
        } else {
            mvprintw(y + i + 1, x, "                    ");
        }
    }

    pthread_mutex_unlock(&chat_log_lock);

    mvprintw(y + visible_lines - 11, x, "Tab으로 채팅 전환 Ctrl+A로 신고모드 전환 / ↑↓ 스크롤");
    draw_debug_messages(y + visible_lines - 12, x);
}

void redraw_chat_input_line(const wchar_t* label) {
    pthread_mutex_lock(&shared_input.lock);
    int y = shared_input.cursor_y;
    int x = shared_input.cursor_x;
    wchar_t input_copy[MAX_INPUT_LEN];
    wcsncpy(input_copy, shared_input.input_buf, MAX_INPUT_LEN);
    pthread_mutex_unlock(&shared_input.lock);

    int label_width = wcswidth(label, -1);
    if (label_width < 0) label_width = wcslen(label);  // fallback

    int input_start_x = x + label_width;

    mvaddwstr(y, x, label);
    mvaddwstr(y, input_start_x, input_copy);
    
    int input_width = wcswidth(input_copy, -1);
    if (input_width < 0) input_width = wcslen(input_copy);  // fallback

    move(y, input_start_x + input_width);
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
    signal(SIGSEGV, listener_signal_handler);
    signal(SIGABRT, listener_signal_handler);
    signal(SIGFPE, listener_signal_handler);
    signal(SIGILL, listener_signal_handler);
    signal(SIGBUS, listener_signal_handler);
    signal(SIGPIPE, SIG_IGN);

    int sock = get_game_sock();
    char readbuf[1024];
    char leftover[4096] = "";

    listener_alive = 1;
    time_t last_card_request = 0;
    
    while (1) {
        if (gameOver) break;

        if (!cards_initialized) {
            if (time(NULL) - last_card_request >= 3) {
                if (!(cards_initialized = are_cards_valid())) {
                    send(sock, "GET_ALL_CARDS\n", strlen("GET_ALL_CARDS\n"), 0);
                    last_card_request = time(NULL);
                }
            }
        }

        // select 준비
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int sel = select(sock + 1, &read_fds, NULL, NULL, &timeout);
        if (sel < 0) {
            perror("❌ [listener_thread] select 실패");
            break;
        } else if (sel == 0) {
            // timeout - 아무것도 안옴
            continue;
        }

        // 읽을 수 있는 데이터가 있음
        int len = recv(sock, readbuf, sizeof(readbuf) - 1, 0);
        if (len <= 0) {
            perror("❌ [listener_thread] recv 실패 또는 연결 종료");
            fprintf(stderr, "📉 time: %ld, sock: %d\n", time(NULL), sock);
            fflush(stderr);
            break;
        }

        readbuf[len] = '\0';
        strncat(leftover, readbuf, sizeof(leftover) - strlen(leftover) - 1);
        
        //set_debug_message(0, leftover); // 디버깅
        char* line_start = leftover;
        char* newline;
        while ((newline = strchr(line_start, '\n')) != NULL) {
            *newline = '\0';
            char* line = line_start;
            //set_debug_message(1, line);

            if (strlen(line) == 0) {
                line_start = newline + 1;
                continue;
            }

            if (strncmp(line, "SESSION_ACK", 11) == 0) {
                send(sock, "GET_ALL_CARDS\n", strlen("GET_ALL_CARDS\n"), 0);
                last_card_request = time(NULL);
            } else if (strncmp(line, "ALL_CARDS|", 10) == 0) {
                
                int success = 1;

                char* payload = strdup(line + 10);

                // 토큰 개수 확인
                int token_count = 0;
                char* check_tok = strtok(payload, "|");
                while (check_tok) {
                    token_count++;
                    check_tok = strtok(NULL, "|");
                }

                if (token_count != MAX_CARDS * 3) {
                    printf("❌ ALL_CARDS 파싱 실패: 토큰 개수 %d (정상: %d)\n", token_count, MAX_CARDS * 3);
                    fflush(stdout);
                    success = 0;
                    free(payload);
                    continue;
                }

                // 다시 파싱 시작 (복사본 재사용 위해 strdup 다시 호출)
                free(payload);
                payload = strdup(line + 10);
                char* token = strtok(payload, "|");

                for (int i = 0; i < MAX_CARDS; i++) {
                    if (!token) { success = 0; break; }
                    strncpy(cards[i].name, token, sizeof(cards[i].name) - 1);
                    cards[i].name[sizeof(cards[i].name) - 1] = '\0';

                    token = strtok(NULL, "|");
                    if (!token) { success = 0; break; }
                    cards[i].type = atoi(token);

                    token = strtok(NULL, "|");
                    if (!token) { success = 0; break; }
                    cards[i].isUsed = atoi(token);

                    token = strtok(NULL, "|");
                }
                
                free(payload);
                if (success) {
                    pthread_mutex_lock(&shared_input.lock);
                    cards_initialized = 1;
                    shared_input.needs_redraw = 1;
                    pthread_mutex_unlock(&shared_input.lock);
                }
            } else if (strncmp(line, "CARD_UPDATE|", 12) == 0) {
                set_debug_message(1, line);
                char* ptr = line + 12;
                char* idx_str = strtok(ptr, "|");
                char* used_str = strtok(NULL, "|");
                if (idx_str && used_str) {
                    int idx = atoi(idx_str);
                    int used = atoi(used_str);
                    if (idx >= 0 && idx < MAX_CARDS) 
                    cards[idx].isUsed = used;
                }
                pthread_mutex_lock(&shared_input.lock);
                shared_input.needs_redraw = 1;
                pthread_mutex_unlock(&shared_input.lock);
            } else if (strncmp(line, "TURN_UPDATE|", 12) == 0) {
                //set_debug_message(1, line);
                char* ptr = line + 12;
                char* tok1 = strtok(ptr, "|");
                char* tok2 = strtok(NULL, "|");
                char* tok3 = strtok(NULL, "|");
                char* tok4 = strtok(NULL, "|");
                if (tok1 && tok2 && tok3 && tok4) {
                    int new_turn_team = atoi(tok1);
                    int new_phase = atoi(tok2);
                    int new_redScore = atoi(tok3);
                    int new_blueScore = atoi(tok4);

                    redScore = new_redScore;
                    blueScore = new_blueScore;
                    
                    // 시스템 코드(점수만 바꾸는 경우 바로 널 리턴)
                    if (new_turn_team == 2) {
                        pthread_mutex_lock(&shared_input.lock);
                        shared_input.needs_redraw = 1;
                        pthread_mutex_unlock(&shared_input.lock);
                    } else {
                        int old_turn_team = turn_team;
                        int old_phase = phase;
                        // 상태 반영
                        turn_team = new_turn_team;
                        phase = new_phase;

                        // ✅ 출력은 이전 턴 정보와 비교해 조건부 실행
                        if (!(new_turn_team == old_turn_team && new_phase == old_phase)) {
                            const char* team_str = (new_turn_team == 0) ? "레드" : "블루";
                            const char* phase_str = (new_phase == 0) ? "팀장" : "팀원";
                            char sys_msg[64];
                            snprintf(sys_msg, sizeof(sys_msg), "[%s팀의 %s 턴입니다.]", team_str, phase_str);
                            append_chat_log(sys_msg);
                        }

                        int my_index = global_info.myPlayerIndex;
                        int my_team = global_info.players[my_index].team;
                        bool is_leader = global_info.players[my_index].is_leader;

                        pthread_mutex_lock(&shared_input.lock);
                        if (my_team == turn_team && is_leader && phase == 0) {
                            shared_input.mode = INPUT_HINT;
                        } else if (my_team == turn_team && !is_leader && phase == 1) {
                            shared_input.mode = INPUT_ANSWER;
                        } else {
                            shared_input.mode = NONE;
                        }

                        shared_input.needs_redraw = 1;
                        pthread_mutex_unlock(&shared_input.lock);
                    }
                    
                }
            } else if (strncmp(line, "HINT|", 5) == 0) {
                char* ptr = line + 5;
                char* team_str = strtok(ptr, "|");
                char* word = strtok(NULL, "|");
                char* count_str = strtok(NULL, "|");
                if (team_str && word && count_str) {
                    turn_team = atoi(team_str);
                    strncpy(hintWord, word, sizeof(hintWord) - 1);
                    hintWord[sizeof(hintWord) - 1] = '\0';
                    hintCount = atoi(count_str);

                    char sys_msg[128];
                    snprintf(sys_msg, sizeof(sys_msg), "[팀장 입력 힌트: %s, 연결 수 - %d, %d번 시도 가능합니다.]", hintWord, hintCount, hintCount);
                    append_chat_log(sys_msg);
                    
                    pthread_mutex_lock(&shared_input.lock);
                    shared_input.needs_redraw = 1;
                    pthread_mutex_unlock(&shared_input.lock);
                }
            } else if (strncmp(line, "CHAT|", 5) == 0) {
                set_debug_message(2, line);
                char* ptr = line + 5;
                char* team_str = strtok(ptr, "|");
                char* leader_str = strtok(NULL, "|");
                char* nickname = strtok(NULL, "|");
                char* content = strtok(NULL, "");
                if (team_str && leader_str && nickname && content) {
                    int sender_team = atoi(team_str);
                    int sender_is_leader = atoi(leader_str);

                    int my_index = global_info.myPlayerIndex;
                    int my_team = global_info.players[my_index].team;
                    bool my_is_leader = global_info.players[my_index].is_leader;

                    bool visible = false;

                    if (sender_team == 2) {
                        // 시스템 메시지
                        visible = true;
                    } else if (sender_team == my_team) {
                        // 같은 팀
                        if (!sender_is_leader) {
                            // 팀원 발언 -> 팀원, 팀장 모두 볼 수 있음
                            visible = true;
                        } else if (sender_is_leader && my_is_leader) {
                            // 팀장 발언 -> 같은 팀 팀장만 볼 수 있음
                            visible = true;
                        }
                        // 팀장 발언 -> 팀원은 X (아무것도 안 함)
                    } else {
                        // 다른 팀
                        if (!sender_is_leader) {
                            // 팀원 <-> 팀원
                            visible = true;
                        } else if (sender_is_leader && my_is_leader) {
                            // 팀장 <-> 팀장
                            visible = true;
                        }
                        // 교차 팀장 <-> 팀원 X

                    }
                    if (visible) {
                        char formatted[256];
                        if (sender_team == 2) {
                            snprintf(formatted, sizeof(formatted), "[%s]", content);
                            append_chat_log(formatted);
                            pthread_mutex_lock(&shared_input.lock);
                            shared_input.needs_redraw = 1;
                            pthread_mutex_unlock(&shared_input.lock);
                        } else {
                            snprintf(formatted, sizeof(formatted), "%s: %s", nickname, content);
                            int color = (sender_team == 0) ? 2 : 3;
                            append_chat_log_colored(formatted, color);
                            pthread_mutex_lock(&shared_input.lock);
                            shared_input.needs_redraw = 1;
                            pthread_mutex_unlock(&shared_input.lock);
                        }
                    }
                }
            } else if (strncmp(line, "REPORT_OK|", 10) == 0) {
                int count = 0;
                char* count_str = line + 10;
                char* suspend_ptr = strchr(count_str, '|');

                if (suspend_ptr) {
                    *suspend_ptr = '\0';
                    count = atoi(count_str);

                    if (strcmp(suspend_ptr + 1, "SUSPENDED") == 0) {
                        // 정지된 경우 안내 메시지 출력
                        char msg[160];
                        snprintf(msg, sizeof(msg), "[신고 완료] 누적 %d회 - 해당 유저는 다음 게임부터 참여가 제한됩니다.", count);
                        append_chat_log(msg);
                    }
                } else {
                    // 일반 신고 성공
                    count = atoi(count_str);
                    char msg[128];
                    snprintf(msg, sizeof(msg), "[신고 완료] 누적 신고 %d회", count);
                    append_chat_log(msg);
                }

    
                report_completed = 1;
                pthread_mutex_lock(&shared_input.lock);
                shared_input.needs_redraw = 1;
                pthread_mutex_unlock(&shared_input.lock);
            } else if (strncmp(line, "GAME_OVER|", 10) == 0) {
                int winner_team = -1;
                    
                // 메시지에서 승리 팀 번호 파싱 // strtok으로 로직 변경할 것
                sscanf(line, "GAME_OVER|%d", &winner_team);
                //set_debug_message(0, line);

                // 내 팀 정보 확인
                int my_index = global_info.myPlayerIndex;
                int team_color = global_info.players[my_index].team;
            
                // 결과 결정 - 선언 중복 해결할 것
                result = (team_color == winner_team) ? RESULT_WIN : RESULT_LOSE;

                pthread_mutex_lock(&shared_input.lock);
                gameOver = 1; // 디버그용 비활성화
                shared_input.needs_redraw = 1;
                pthread_mutex_unlock(&shared_input.lock);

            } else if (strncmp(line, "REPORT_ERROR", 12) == 0) {
                append_chat_log("[신고 실패] 요청을 처리할 수 없습니다.");
                report_completed = -1;
                pthread_mutex_lock(&shared_input.lock);
                shared_input.needs_redraw = 1;
                pthread_mutex_unlock(&shared_input.lock);
            }

            line_start = newline + 1;
        }

        size_t remaining = strlen(line_start);
        if (remaining >= sizeof(leftover)) remaining = sizeof(leftover) - 1;
        memmove(leftover, line_start, remaining);
        leftover[remaining] = '\0';
    }

    listener_alive = 0;
    return NULL;
}

void update_input_position_for_mode(InputMode mode, int term_y, int term_x, int chat_y, int board_offset_x) {
    pthread_mutex_lock(&shared_input.lock);
    shared_input.cursor_y = term_y - 2;  // 기본값 (예: 화면 하단 입력)
    shared_input.cursor_x = 2;

    switch (mode) {
        case INPUT_CHAT:
            shared_input.cursor_y = chat_y + 14;
            shared_input.cursor_x = 2;
            break;
        case INPUT_HINT:
            shared_input.cursor_y = chat_y + 14;
            shared_input.cursor_x = board_offset_x;
            break;
        case INPUT_LINK:
            shared_input.cursor_y = chat_y + 14;
            shared_input.cursor_x = board_offset_x;
            break;
        case INPUT_ANSWER:
            shared_input.cursor_y = chat_y + 14;
            shared_input.cursor_x = board_offset_x;
            break;
        case INPUT_REPORT:
            shared_input.cursor_y = term_y - 2;
            shared_input.cursor_x = 4;
            break;
        default: break;
    }

    pthread_mutex_unlock(&shared_input.lock);
}

static void sigwinch_handler(int signo) {
    endwin();           // ncurses 내부 상태 초기화
    refresh();          // 화면 새로고침
    clear();            // 버퍼 초기화
    resize_term(0, 0);  // 새로운 터미널 크기 인식

    // 단순히 redraw 플래그를 세우면 screen_redraw()가 다음 루프에서 처리
    pthread_mutex_lock(&shared_input.lock);
    shared_input.needs_redraw = 1;
    pthread_mutex_unlock(&shared_input.lock);
}

void screen_redraw() {
    pthread_mutex_lock(&shared_input.lock);
    if(!shared_input.needs_redraw) {
        pthread_mutex_unlock(&shared_input.lock);
        return;
    } 
    shared_input.needs_redraw = 0;
    pthread_mutex_unlock(&shared_input.lock);
    // 게임 종료 처리 - 화면 이동 필요
    if (gameOver) {
        gameLoop = 0;
        endwin();
        return;
    }
    
    //erase(); 
    clear(); // 디버깅
    
    // 화면 크기 가져오기
    int term_y, term_x;
    getmaxyx(stdscr, term_y, term_x);
    (void)term_y;

    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", local);

    // mvprintw(1, term_x - 20, "⏰ %s", time_str);

    // 보드 위치 계산
    int board_offset_x = (term_x - (12 * BOARD_SIZE)) / 2;
    int board_offset_y = 2;
    int chat_y = board_offset_y + BOARD_SIZE * 4 + 2;

    // 현재 턴 팀과 역할 출력
    const char* team = (turn_team == 0) ? "레드팀" : "블루팀";
    const char* role = (phase == 0) ? "팀장" : "팀원";
    mvprintw(0, 2, "[%s %s 차례]", team, role);
    
    int my_index = global_info.myPlayerIndex;
    int my_team = global_info.players[my_index].team;
    bool is_leader = global_info.players[my_index].is_leader;

    bool is_my_turn = false;
    if (my_team == turn_team) {
        if ((phase == 0 && is_leader) || (phase == 1 && !is_leader)) {
            is_my_turn = true;
        }
    }
    
    pthread_mutex_lock(&shared_input.lock);
    InputMode current_mode = shared_input.mode;
    pthread_mutex_unlock(&shared_input.lock);

    const wchar_t* guide_label = L"";
    switch (current_mode) {
        case INPUT_CHAT:   guide_label = L"채팅 입력:"; break;
        case INPUT_HINT:   guide_label = L"힌트를 입력하세요: "; break;
        case INPUT_LINK:   guide_label = L"연결 수를 입력하세요: "; break;
        case INPUT_ANSWER: guide_label = L"정답을 입력하세요:"; break;
        default: break;
    }

    // UI 출력
    draw_team_ui(2, 2, redScore, blueScore);
    draw_board(board_offset_x, board_offset_y, global_info.players[global_info.myPlayerIndex].is_leader);
    draw_chat_ui(chat_y, 2);

    curs_set((is_my_turn || current_mode == INPUT_CHAT) ? 1 : 0);
    update_input_position_for_mode(current_mode, term_y, term_x, chat_y, board_offset_x);
    redraw_chat_input_line(guide_label);

    usleep(20000);
    refresh();
    //wnoutrefresh(stdscr);
    //doupdate();
}

SceneState codenames_screen(GameInitInfo info) {
    global_info = info;

    setlocale(LC_ALL, "");
    //setvbuf(stdout, NULL, _IONBF, 0);
    initscr(); 
    signal(SIGWINCH, sigwinch_handler);
    cbreak(); noecho(); keypad(stdscr, TRUE); start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_WHITE, COLOR_RED);
    init_pair(3, COLOR_WHITE, COLOR_BLUE);
    init_pair(4, COLOR_WHITE, COLOR_MAGENTA);

    int sock = get_game_sock();

    pthread_t tid;
    int err = pthread_create(&tid, NULL, listener_thread, NULL);
    if (err != 0) {
        fprintf(stderr, "❌ listener_thread 생성 실패: %s\n", strerror(err));
    }
    
    pthread_t input_tid;
    pthread_create(&input_tid, NULL, input_thread_func, NULL);

    pthread_detach(input_tid);
    pthread_detach(tid);
    
    time_t last_tick = 0;

    while (gameLoop) {
        /*if (!listener_alive) {
            mvprintw(1, 2, "❌ listener_thread가 비정상 종료됨 (네트워크 문제?)");
        }*/

        // ✅ 입력 처리: 입력이 준비되었는가?
        pthread_mutex_lock(&shared_input.lock);

        if (shared_input.ready) {
            wchar_t input[MAX_INPUT_LEN];
            wcscpy(input, shared_input.input_buf);
            InputMode mode = shared_input.mode;
            shared_input.ready = 0;
            wmemset(shared_input.input_buf, 0, MAX_INPUT_LEN);  // 💡 버퍼 초기화
            pthread_mutex_unlock(&shared_input.lock);
            
            char converted[180];
            if (wcstombs(converted, input, sizeof(converted)) == (size_t)-1) {
                converted[0] = '\0'; // 에러 처리
                set_debug_message(0, "변환 오류");
            } 

            switch (mode) {
                case INPUT_CHAT: {
                    if (strlen(converted) > 0) {
                        char msg[200];
                        snprintf(msg, sizeof(msg), "CHAT|%s\n", converted);
                        send(sock, msg, strlen(msg), 0);
                        set_debug_message(0, msg);
                    }
                    break;
                }
                case INPUT_HINT:
                    strncpy(hintWord, converted, sizeof(hintWord));
                    shared_input.mode = INPUT_LINK;
                    break;
                case INPUT_LINK:
                    hintCount = atoi(converted);
                    {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "HINT|%s|%d\n", hintWord, hintCount);
                        send(sock, msg, strlen(msg), 0);
                        set_debug_message(0, msg);
                    }
                    shared_input.mode = NONE;
                    break;
                case INPUT_ANSWER:
                    strncpy(answer_text, converted, sizeof(answer_text));
                    {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "ANSWER|%s\n", answer_text);
                        send(sock, msg, strlen(msg), 0);
                        set_debug_message(0, msg);
                    }
                    break;
                case INPUT_REPORT:
                    const char* nickname = global_info.players[report_selected_index].nickname;
                    {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "REPORT|%s|%s\n", token, nickname);
                        send(sock, msg, strlen(msg), 0);
                        set_debug_message(0, msg);
                    }
                    shared_input.mode = INPUT_CHAT;
                    break;
                default: break;
            }

        } else {
            pthread_mutex_unlock(&shared_input.lock);
        }

        /*time_t now = time(NULL);

        if (now != last_tick) {
            last_tick = now;

            pthread_mutex_lock(&shared_input.lock);
            shared_input.needs_redraw = 1;
            pthread_mutex_unlock(&shared_input.lock);
        }*/

        screen_redraw();
    }

    usleep(50000);
    //endwin();
    return SCENE_RESULT;
}
