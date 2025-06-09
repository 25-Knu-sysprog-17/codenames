// 수정된 m7.c (Ctrl 키 포커스, 입력 전환 규칙 및 오류 수정 포함)
//#include <ncurses.h>
#define _XOPEN_SOURCE_EXTENDED

#include "client.h"
#include "codenames_screen.h"
#include <ncursesw/ncurses.h>
#include <wchar.h>
#include <wctype.h>
#include <locale.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BOARD_SIZE 5
#define LEFT_PANEL_WIDTH 35
#define MAX_CARDS 25
#define RED_TEAM 0
#define BLUE_TEAM 1
#define RED_CARD 1
#define BLUE_CARD 2
#define NEUTRAL_CARD 3
#define ASSASSIN_CARD 4
#define CHAT_LOG_SIZE 100
#define MAX_INPUT_LEN 100

typedef struct {
    char name[32];
    int type;
    int isUsed;
} GameCard;

typedef struct {
    char nickname[64];
    int role_num;
    int team;
    int is_leader;
} PlayerEntry;

typedef struct {
    PlayerEntry players[6];
    int myPlayerIndex;
} GameInitInfo;

typedef struct {
    char userName[30];
    int userNum; // 0:레드팀장 1~2:레드팀원 3:블루팀장 4~5:블루팀원
} User;

typedef struct {
    GameCard cards[MAX_CARDS];
    char redTeamWords[9][20];
    char blueTeamWords[8][20];
    int redTeamScore;
    int blueTeamScore;
    int turn;
    int currentTries;
    int gameOver;
    char hintWord[20];
    int hintNumber;
} Game;

User user[6];

char answer_text[32] = "";
char chat_log[CHAT_LOG_SIZE][128] = { "" };
int chat_count = 0;
int chat_scroll_offset = 0;
int report_selected_index = 0;
int last_input_stage = 0;  // 0: 힌트, 1: 링크, 2: 앤서

typedef enum { NONE, INPUT_HINT, INPUT_LINK, INPUT_ANSWER, INPUT_CHAT, INPUT_REPORT } InputMode;
InputMode input_mode = NONE;

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

void draw_board(int offset_x, int offset_y, Game *game, bool reveal_all) {
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            int index = y * BOARD_SIZE + x;
            int draw_y = offset_y + y * 4;
            int draw_x = offset_x + x * 12;
            bool is_revealed = reveal_all || game->cards[index].isUsed;
            draw_card(draw_y, draw_x, game->cards[index].name, game->cards[index].type, is_revealed, game->cards[index].isUsed);
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
        mvprintw(y + 1 + i, x + 2, "%s     신고", user[i].userName);
        if (input_mode == INPUT_REPORT && report_selected_index == i)
            attroff(A_REVERSE);
    }

    attron(COLOR_PAIR(3));
    mvprintw(y + 5, x, "파랑 팀      남은 단어 - %d", 8 - blue);
    attroff(COLOR_PAIR(3));

    for (int i = 3; i < 6; i++) {
        if (input_mode == INPUT_REPORT && report_selected_index == i)
            attron(A_REVERSE);
        mvprintw(y + 1 + i + 2, x + 2, "%s      신고", user[i].userName);
        if (input_mode == INPUT_REPORT && report_selected_index == i)
            attroff(A_REVERSE);
    }
}

void draw_chat_ui(int y, int x) {
    y++; 
    mvprintw(y, x, "-------------------<채팅 로그>-------------------");
    
    int visible_lines = 10;
    int start = chat_count > visible_lines ? chat_count - visible_lines - chat_scroll_offset : 0;
    for (int i = 0; i < visible_lines; i++) {
        int idx = start + i;
        if (idx < chat_count) {
            mvprintw(y + i + 1, x, "%s", chat_log[idx]);
        } else {
            mvprintw(y + i + 1, x, "                    ");
        }
    }
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

void handle_chat_input(int y, int x, char *name) {
    wchar_t chat_input[MAX_INPUT_LEN] = L""; // 유니코드 문자열 입력 버퍼
    mvprintw(y, x, "채팅 입력:");
    refresh();

    int len = 0;
    wint_t wch;
    curs_set(1); // 커서 보이기

    while (1) {
        int result = get_wch(&wch); // 유니코드 입력 처리
        if (result == ERR) continue;
        if (wch == L'\n') break; // Enter 입력 시 종료
        else if (wch == L'\t') { // Tab → 입력 모드 복귀
            if (input_mode == INPUT_CHAT) {
                if (last_input_stage == 0) input_mode = INPUT_HINT;
                else if (last_input_stage == 1) input_mode = INPUT_LINK;
                else if (last_input_stage == 2) input_mode = INPUT_ANSWER;
            } else {
                input_mode = INPUT_CHAT;
            }
            return;
        } else if (wch == 1) { // Ctrl+A → 신고 모드
            input_mode = INPUT_REPORT;
            return;
        } else if (wch == KEY_BACKSPACE || wch == 127) {
            if (len > 0) {
                len--;
                chat_input[len] = L'\0';
                redraw_chat_input_line(y, x + 10, chat_input);
            }
        } else if (wch == KEY_UP) { // 채팅 스크롤 위로
            if (chat_scroll_offset + 1 < chat_count)
                chat_scroll_offset++;
            return;
        } else if (wch == KEY_DOWN) { // 채팅 스크롤 아래로
            if (chat_scroll_offset > 0)
                chat_scroll_offset--;
            return;
        } else if (iswprint(wch) && len < MAX_INPUT_LEN - 1) {
            chat_input[len++] = wch;
            chat_input[len] = L'\0';
            redraw_chat_input_line(y, x + 10, chat_input);
        }
        // move(y, x + 10 + wcslen(chat_input)); // 커서 이동
        // refresh();
    }

    curs_set(0); // 커서 숨기기

    if (wcslen(chat_input) > 0) {
        char msg[128];
        char converted[MAX_INPUT_LEN];
        wcstombs(converted, chat_input, sizeof(converted)); // wide → multi-byte 변환
        snprintf(msg, sizeof(msg), "%s: %s", name, converted); // 최종 메시지 구성

        // 채팅 로그 추가
        if (chat_count < CHAT_LOG_SIZE) {
            strcpy(chat_log[chat_count++], msg);
        } else {
            for (int i = 1; i < CHAT_LOG_SIZE; i++) {
                strcpy(chat_log[i - 1], chat_log[i]);
            }
            strcpy(chat_log[CHAT_LOG_SIZE - 1], msg);
        }
        chat_scroll_offset = 0;
    }
}

void append_chat_log(const char* msg) {
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
}


void readWordsFromFile(char *filename, char wordList[MAX_CARDS][20]) {
    FILE *file = fopen(filename, "r");
    if (!file) exit(1);
    for (int i = 0; i < MAX_CARDS; i++) fscanf(file, "%s", wordList[i]);
    fclose(file);
}

void initGame(Game *game) {
    char wordList[MAX_CARDS][20];
    readWordsFromFile("words.txt", wordList);
    int used[MAX_CARDS] = {0};
    srand(time(NULL));
    for (int i = 0; i < MAX_CARDS; i++) {
        int r;
        do { r = rand() % MAX_CARDS; } while (used[r]);
        used[r] = 1;
        strcpy(game->cards[i].name, wordList[r]);
        game->cards[i].isUsed = 0;
        game->cards[i].type = (i < 9) ? RED_CARD : (i < 17) ? BLUE_CARD : (i < 24) ? NEUTRAL_CARD : ASSASSIN_CARD;
    }
    game->redTeamScore = 0;
    game->blueTeamScore = 0;
    game->turn = RED_TEAM;
    game->currentTries = 0;
    game->gameOver = 0;
    game->hintWord[0] = '\0';
    game->hintNumber = 0;

    for (int i = 0; i < 6; i++) {
        char temp[30];
        sprintf(temp, "guest%d", i);
        strcpy(user[i].userName, temp);
    }
}

void checkGameOver(Game *game) {
    if (game->redTeamScore == 9 || game->blueTeamScore == 8) {
        game->gameOver = 1;
    }
}

int main() {
    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_WHITE, COLOR_RED);
    init_pair(3, COLOR_WHITE, COLOR_BLUE);
    init_pair(4, COLOR_WHITE, COLOR_MAGENTA);

    Game game;
    initGame(&game);

    int phase = 0;
    append_chat_log("[게임 시작!]");
    append_chat_log("[레드팀의 턴입니다.]");
    while (1) {
        clear();
        int term_y, term_x;
        getmaxyx(stdscr, term_y, term_x);
        int board_offset_x = (term_x - (12 * BOARD_SIZE)) / 2;
        int board_offset_y = 2;

        if (game.gameOver) {
            mvprintw(0, 2, "게임 종료! 빨강팀 %d점, 파랑팀 %d점", game.redTeamScore, game.blueTeamScore);
            if (game.redTeamScore == 9 || (game.redTeamScore != 9 && game.turn == RED_TEAM))
                mvprintw(3, 4, "레드팀 승리!");
            else
                mvprintw(3, 4, "블루팀 승리!");
            refresh(); getch(); break;
        }

        const char* team = (game.turn == RED_TEAM) ? "빨강팀" : "파랑팀";
        const char* role = (phase % 2 == 0) ? "팀장" : "팀원";
        mvprintw(0, 2, "[%s %s 차례]", team, role);

        draw_team_ui(2, 2, game.redTeamScore, game.blueTeamScore);
        bool reveal_all = (phase % 2 == 0);
        draw_board(board_offset_x, board_offset_y, &game, reveal_all);

        int chat_y = board_offset_y + BOARD_SIZE * 4 + 2;
        draw_chat_ui(chat_y, 2);

        refresh();
        
        if (input_mode == INPUT_CHAT) {
            handle_chat_input(chat_y + 14, 2, user[0].userName);
            continue;
        }

        if (input_mode == INPUT_REPORT) {
            int ch = getch();
            if (ch == KEY_UP) {
                if (report_selected_index > 0)
                    report_selected_index--;
            } else if (ch == KEY_DOWN) {
                if (report_selected_index < 5)
                    report_selected_index++;
            } else if (ch == 10) {
                char msg[128];
                snprintf(msg, sizeof(msg), "[신고 완료] %s 님을 신고했습니다.", user[report_selected_index].userName);

                if (chat_count < CHAT_LOG_SIZE) {
                    strcpy(chat_log[chat_count++], msg);
                } else {
                    for (int i = 1; i < CHAT_LOG_SIZE; i++) {
                        strcpy(chat_log[i - 1], chat_log[i]);
                    }
                    strcpy(chat_log[CHAT_LOG_SIZE - 1], msg);
                }

                chat_scroll_offset = 0;
                input_mode = INPUT_CHAT;  // ✅ 바로 채팅모드로 전환
            } else if (ch == 9) {
                input_mode = INPUT_CHAT;
            } else if (ch == 1) {
                input_mode = INPUT_HINT;
            }
            continue;
        }

        int y = board_offset_y + BOARD_SIZE * 4;

        if (phase % 2 == 0) {
            mvprintw(y, board_offset_x, "힌트를 입력하세요: ");
            input_mode = INPUT_HINT;
            handle_dynamic_input(game.hintWord, sizeof(game.hintWord), y, board_offset_x + 20);
            if (input_mode != INPUT_HINT) continue;

            mvprintw(y + 1, board_offset_x, "연결 수를 입력하세요: ");
            char temp[4] = "";
            input_mode = INPUT_LINK;
            handle_dynamic_input(temp, sizeof(temp), y + 1, board_offset_x + 23);
            if (input_mode != INPUT_LINK || strlen(temp) == 0) continue;

            game.hintNumber = atoi(temp);
            game.currentTries = 0;
            phase++;
        } else {
            mvprintw(y, board_offset_x, "정답: ");
            input_mode = INPUT_ANSWER;
            handle_dynamic_input(answer_text, sizeof(answer_text), y, board_offset_x + 5);
            if (input_mode != INPUT_ANSWER) continue;

            int found = 0;
            for (int i = 0; i < MAX_CARDS; i++) {
                if (!game.cards[i].isUsed && strcmp(game.cards[i].name, answer_text) == 0) {
                    game.cards[i].isUsed = 1; found = 1;
                    int t = game.cards[i].type;
                    if (game.turn == RED_TEAM) {

                        if (t == RED_CARD) {
                            append_chat_log("[정답! 레드팀 득점]");
                            game.redTeamScore++;
                            game.currentTries++;
                        } else {
                            if (t == BLUE_CARD) 
                            {
                                append_chat_log("[오답! 블루팀 득점]");
                                game.blueTeamScore++;
                            }else{
                                append_chat_log("[일반카드! 차례가 넘어갑니다.]");
                                game.turn = BLUE_TEAM; phase = 2;
                            }
                            append_chat_log("[블루팀의 턴입니다.]");
                            

                        }
                        if (t == ASSASSIN_CARD) game.gameOver = 1;
                    } else {

                        if (t == BLUE_CARD) {
                            append_chat_log("[정답! 블루팀 득점]");
                            game.blueTeamScore++;
                            game.currentTries++;
                        } else {
                            if (t == RED_CARD) 
                            {
                                append_chat_log("[오답! 레드팀 득점]");
                                game.redTeamScore++;
                            }else{
                                append_chat_log("[일반카드! 차례가 넘어갑니다.]");
                                game.turn = RED_TEAM; phase = 0;
                            }
                            append_chat_log("[레드팀의 턴입니다.]");
                        }
                        if (t == ASSASSIN_CARD) game.gameOver = 1;
                    }
                    checkGameOver(&game);
                    if (game.currentTries >= game.hintNumber + 1) {
                        game.turn = (game.turn == RED_TEAM) ? BLUE_TEAM : RED_TEAM;
                        phase = (game.turn == RED_TEAM) ? 0 : 2;
                    }
                    break;
                }
            }
            if (!found) {
                mvprintw(y + 1, board_offset_x, "단어를 찾을 수 없습니다.");
                getch();
            }
        }
        input_mode = NONE;
    }

    endwin();
    return 0;
}