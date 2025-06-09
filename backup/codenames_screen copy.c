// 네트워크 기반으로 리팩토링된 codenames_screen.c
#define _XOPEN_SOURCE 700

#include "client.h"
#include "codenames_screen.h"
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

static GameCard cards[MAX_CARDS];
static int redScore = 0;
static int blueScore = 0;
static int turn_team = 0;
static int phase = 0;
static int gameOver = 0;
static char hintWord[32] = "";
static int hintCount = 0;

static char answer_text[32] = "";
static char chat_log[CHAT_LOG_SIZE][128] = { "" };
static int chat_count = 0;
static int chat_scroll_offset = 0;
static int report_selected_index = 0;

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
} SharedInput;

static SharedInput shared_input = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .input_buf = { 0 },
    .ready = 0,
    .mode = NONE
};

void listener_signal_handler(int sig) {
    fprintf(stderr, "\n💥 [listener_thread] 크래시 감지됨: 시그널 %d (%s)\n", sig, strsignal(sig));
    fflush(stderr);
    _exit(1);
}

void* input_thread_func(void* arg) {
    wint_t wch;
    wchar_t local_buf[MAX_INPUT_LEN];
    int len = 0;

    while (1) {
        wmemset(local_buf, 0, MAX_INPUT_LEN);
        if (get_wch(&wch) == ERR) continue;

        pthread_mutex_lock(&shared_input.lock);
        
        // mvprintw(1, 1, "입력됨: %lc (code: %d) ", wch, wch); - 디버깅용
        // refresh();
        
        // ✅ 채팅 스크롤 조절
        if (shared_input.mode == INPUT_CHAT && (wch == KEY_UP || wch == KEY_DOWN)) {
            pthread_mutex_unlock(&shared_input.lock); // 잠금 해제
            pthread_mutex_lock(&chat_log_lock);
            if (wch == KEY_UP && chat_scroll_offset + 1 < chat_count)
                chat_scroll_offset++;
            else if (wch == KEY_DOWN && chat_scroll_offset > 0)
                chat_scroll_offset--;
            pthread_mutex_unlock(&chat_log_lock);
            continue;
        }

        if (wch == L'\n') {
            if (len >= MAX_INPUT_LEN) len = MAX_INPUT_LEN - 1;
            local_buf[len] = L'\0';  // 안전하게 종료 문자 삽입

            wcsncpy(shared_input.input_buf, local_buf, MAX_INPUT_LEN - 1);
            shared_input.input_buf[MAX_INPUT_LEN - 1] = L'\0';  // 명시적 종료

            shared_input.ready = 1;
            shared_input.needs_redraw = 1;
            len = 0;
        } else if (wch == 127 || wch == KEY_BACKSPACE) {
            if (len > 0) len--;
        } else if (wch == L'\t') {  // Tab → 채팅 모드 전환
            shared_input.mode = INPUT_CHAT;
            shared_input.ready = 1;
            shared_input.needs_redraw = 1;
            len = 0;
        } else if (wch == 1) {     // Ctrl+A → 신고 모드
            shared_input.mode = INPUT_REPORT;
            shared_input.ready = 1;
            shared_input.needs_redraw = 1;
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
    mvprintw(y + 5, x, "레드 팀      남은 단어 - %d", 8 - blue);
    attroff(COLOR_PAIR(3));

    for (int i = 3; i < 6; i++) {
        if (current_mode == INPUT_REPORT && report_selected_index == i)
            attron(A_REVERSE);
        mvprintw(y + 1 + i + 2, x + 2, "%s      신고", global_info.players[i].nickname);
        if (current_mode == INPUT_REPORT && report_selected_index == i)
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

void redraw_chat_input_line(int y, int x) {
    wchar_t copy[MAX_INPUT_LEN];
    pthread_mutex_lock(&shared_input.lock);
    wcsncpy(copy, shared_input.input_buf, MAX_INPUT_LEN);
    pthread_mutex_unlock(&shared_input.lock);

    int width = wcswidth(copy, -1);
    if (width < 0) width = 0;
    move(y, x);
    clrtoeol();
    mvaddwstr(y, x, copy);
    move(y, x + width);
    refresh();
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
    printf("[listener_thread 시작됨] socket=%d\n", sock);
    fflush(stdout);

    listener_alive = 1;
    time_t last_card_request = 0;
    int cards_initialized = 0;

    while (1) {
        if (gameOver) break;

        if (!cards_initialized && time(NULL) - last_card_request >= 3) {
            send(sock, "GET_ALL_CARDS\n", strlen("GET_ALL_CARDS\n"), 0);
            last_card_request = time(NULL);
            printf("🔁 GET_ALL_CARDS 요청 재전송\n");
            fflush(stdout);
        }

        int len = recv(sock, readbuf, sizeof(readbuf) - 1, 0);
        if (len <= 0) {
            perror("❌ [listener_thread] recv 실패 또는 연결 종료");
            fprintf(stderr, "📉 time: %ld, sock: %d\n", time(NULL), sock);
            fflush(stderr);
            break;
        }

        readbuf[len] = '\0';
        printf("📥 수신 원본 ↓↓↓ (%d bytes)\n%s\n---------------------\n", len, readbuf);
        fflush(stdout);

        strncat(leftover, readbuf, sizeof(leftover) - strlen(leftover) - 1);

        // 🧪 leftover 상태 진단
        printf("🧪 leftover 누적 상태: [%s]\n", leftover);
        if (strstr(leftover, "ALL_CARDS|") && strchr(leftover, '\n') == NULL) {
            printf("⚠️ ALL_CARDS 쪼개짐 감지: 개행 없음, 다음 recv까지 대기중\n");
        }
        fflush(stdout);

        char* line_start = leftover;
        char* newline;
        while ((newline = strchr(line_start, '\n')) != NULL) {
            *newline = '\0';
            char* line = line_start;

            if (strlen(line) == 0) {
                line_start = newline + 1;
                continue;
            }

            if (strncmp(line, "SESSION_ACK", 11) == 0) {
                send(sock, "GET_ALL_CARDS\n", strlen("GET_ALL_CARDS\n"), 0);
                last_card_request = time(NULL);
            } else if (strncmp(line, "ALL_CARDS|", 10) == 0) {
                int success = 1;

                // 🔍 사전 토큰 수 검사 (75개 이상이어야 정상)
                char* tmp_copy = strdup(line + 10);
                int token_count = 0;
                char* check_tok = strtok(tmp_copy, "|");
                while (check_tok) {
                    token_count++;
                    check_tok = strtok(NULL, "|");
                }
                free(tmp_copy);

                if (token_count != MAX_CARDS * 3) {
                    printf("❌ ALL_CARDS 파싱 실패: 토큰 개수 %d (정상: 75)\n", token_count);
                    fflush(stdout);
                    continue;
                }

                // ✅ 본격 파싱 시작
                char* token = strtok(line + 10, "|");
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

                    token = strtok(NULL, "|");  // 다음으로 진행
                }

                if (success) {
                    cards_initialized = 1;
                    printf("✅ ALL_CARDS 수신 완료\n");
                    fflush(stdout);
                } else {
                    printf("❌ ALL_CARDS 파싱 실패 (도중 누락 발생)\n");
                    fflush(stdout);
                }
            }

            if (strncmp(line, "CARD_UPDATE|", 12) == 0) {
                char* ptr = line + 12;
                char* idx_str = strtok(ptr, "|");
                char* used_str = strtok(NULL, "|");
                if (idx_str && used_str) {
                    int idx = atoi(idx_str);
                    int used = atoi(used_str);
                    if (idx >= 0 && idx < MAX_CARDS) cards[idx].isUsed = used;
                }
            } else if (strncmp(line, "TURN_UPDATE|", 12) == 0) {
                char* ptr = line + 12;
                char* tok1 = strtok(ptr, "|");
                char* tok2 = strtok(NULL, "|");
                char* tok3 = strtok(NULL, "|");
                char* tok4 = strtok(NULL, "|");
                if (tok1 && tok2 && tok3 && tok4) {
                    turn_team = atoi(tok1);
                    phase = atoi(tok2);
                    redScore = atoi(tok3);
                    blueScore = atoi(tok4);

                    char sys_msg[64];
                    snprintf(sys_msg, sizeof(sys_msg), "[%s팀의 턴입니다.]", turn_team == 0 ? "레드" : "블루");
                    append_chat_log(sys_msg);

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
                    snprintf(sys_msg, sizeof(sys_msg), "[팀장 입력 힌트: %s]", hintWord);
                    append_chat_log(sys_msg);
                }
            } else if (strncmp(line, "CHAT|", 5) == 0) {
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
                    if (sender_team == my_team) {
                        if ((sender_is_leader && my_is_leader) || (!sender_is_leader && !my_is_leader)) {
                            visible = true;
                        }
                    } else if (sender_team == 3) {
                        visible = true;
                    } else {
                        if (sender_is_leader && my_is_leader) {
                            visible = true;
                        }
                    }

                    if (visible) {
                        char formatted[256];
                        if (sender_team == 3) {
                            snprintf(formatted, sizeof(formatted), "[%s]", content);
                            append_chat_log(formatted);
                        } else {
                            snprintf(formatted, sizeof(formatted), "%s: %s", nickname, content);
                            int color = (sender_team == 0) ? 2 : 3;
                            append_chat_log_colored(formatted, color);
                        }
                    }
                }
            } else if (strncmp(line, "GAME_OVER|", 10) == 0) {
                gameOver = 1;
            }

            line_start = newline + 1;
        }

        // 처리 후 남은 leftover 정리
        size_t remaining = strlen(line_start);
        if (remaining >= sizeof(leftover)) remaining = sizeof(leftover) - 1;
        memmove(leftover, line_start, remaining);
        leftover[remaining] = '\0';
    }

    listener_alive = 0;
    return NULL;
}

SceneState codenames_screen(GameInitInfo info) {
    global_info = info;

    setlocale(LC_ALL, "");
    setvbuf(stdout, NULL, _IONBF, 0);
    initscr(); 
    
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

    // append_chat_log("[게임 시작!]");
    // char init_msg[64];
    // snprintf(init_msg, sizeof(init_msg), "[%s팀의 턴입니다.]", turn_team == 0 ? "레드" : "블루");
    // append_chat_log(init_msg);
    
    while (1) {
        erase();

        if (!listener_alive) {
            mvprintw(1, 2, "❌ listener_thread가 비정상 종료됨 (네트워크 문제?)");
        }

        // 화면 크기 가져오기
        int term_y, term_x;
        getmaxyx(stdscr, term_y, term_x);
        (void)term_y;

        // 보드 위치 계산
        int board_offset_x = (term_x - (12 * BOARD_SIZE)) / 2;
        int board_offset_y = 2;
        int chat_y = board_offset_y + BOARD_SIZE * 4 + 2;

        // 게임 종료 처리 - 화면 이동 필요
        if (gameOver) {
            mvprintw(0, 2, "게임 종료! 레드팀 %d점, 블루팀 %d점", redScore, blueScore);
            mvprintw(2, 4, redScore >= 9 ? "레드팀 승리!" : "블루팀 승리!");
            refresh();
            getch();
            break;
        }

        // 현재 턴 팀과 역할 출력
        const char* team = (turn_team == 0) ? "레드팀" : "블루팀";
        const char* role = (phase == 0) ? "팀장" : "팀원";
        mvprintw(0, 2, "[%s %s 차례]", team, role);

        // UI 출력
        draw_team_ui(2, 2, redScore, blueScore);
        draw_board(board_offset_x, board_offset_y, global_info.players[global_info.myPlayerIndex].is_leader);
        draw_chat_ui(chat_y, 2);
        
        // redraw
        pthread_mutex_lock(&shared_input.lock);
        if (shared_input.needs_redraw) {
            redraw_chat_input_line(chat_y + 14, 2);
            shared_input.needs_redraw = 0;
        }
        pthread_mutex_unlock(&shared_input.lock);

        // ✅ 입력 처리: 입력이 준비되었는가?
        pthread_mutex_lock(&shared_input.lock);

        if (shared_input.ready) {
            wchar_t input[MAX_INPUT_LEN];
            wcscpy(input, shared_input.input_buf);
            InputMode mode = shared_input.mode;
            shared_input.ready = 0;
            pthread_mutex_unlock(&shared_input.lock);

            char converted[128];
            if (wcstombs(converted, input, sizeof(converted)) == (size_t)-1) {
                converted[0] = '\0'; // 에러 처리
            }

            switch (mode) {
                case INPUT_CHAT: {
                    if (strlen(converted) > 0) {
                        char msg[160];
                        snprintf(msg, sizeof(msg), "CHAT|%s\n", converted);
                        send(sock, msg, strlen(msg), 0);
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
                    }
                    break;
                case INPUT_ANSWER:
                    strncpy(answer_text, converted, sizeof(answer_text));
                    {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "ANSWER|%s\n", answer_text);
                        send(sock, msg, strlen(msg), 0);
                    }
                    break;
                case INPUT_REPORT:
                    // 🔒 신고 대상 선택 조작 생략됨 (선택 불가 고정)
                    // int ch = getch(); ... 등 생략
                    {
                        // int ch = getch();
                        // if (ch == KEY_UP && report_selected_index > 0) report_selected_index--;
                        // else if (ch == KEY_DOWN && report_selected_index < 5) report_selected_index++;
                        // else if (ch == 10) {
                        //     char msg[128];
                        //     snprintf(msg, sizeof(msg), "[신고 완료] %s 님을 신고했습니다.", global_info.players[report_selected_index].nickname);
                        //     append_chat_log(msg);
                        //     input_mode = INPUT_CHAT;
                        // } else if (ch == 9) input_mode = INPUT_CHAT;
                        // else if (ch == 1) input_mode = INPUT_HINT;
                        // continue;
                        //input_mode = NONE;
                    }
                    break;
                default: break;
            }

        } else {
            const char* guide_msg = NULL;
            switch (shared_input.mode) {
                case INPUT_HINT:
                    guide_msg = "힌트를 입력하세요: ";
                    break;
                case INPUT_LINK:
                    guide_msg = "연결 수를 입력하세요: ";
                    break;
                case INPUT_ANSWER:
                    guide_msg = "정답을 입력하세요:";
                    break;
                default: break;
            }
            pthread_mutex_unlock(&shared_input.lock);

            if (guide_msg) {
                mvprintw(chat_y + 13, board_offset_x, "%s", guide_msg);
            }
        }

        usleep(150000);  // UI 과부하 방지용 sleep
        refresh();
    }

    endwin();
    return SCENE_RESULT;
}
