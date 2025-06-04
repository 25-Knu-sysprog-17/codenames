#include "connect_to_server.h"
#include "gui_util.h"
#include "login_screen.h"
#include "waiting_screen.h"
#include "lobby_screen.h"
#include <ncurses.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <locale.h>
#include <stdlib.h> //test

#define MAX_INPUT 32

typedef enum {
    STATE_ID_INPUT,
    STATE_PW_INPUT,
    STATE_LOGIN_BUTTON,
    STATE_SIGNUP_BUTTON
} UIState;

static char id_buf[MAX_INPUT] = {0};
static char pw_buf[MAX_INPUT] = {0};
static int id_y, id_x;
static int pw_y, pw_x;
UIState current_state = STATE_ID_INPUT;

static bool login_success = false;

void draw_connection_status(int y, int x) {
    start_color();
    init_pair(10, COLOR_GREEN, COLOR_BLACK); // 성공
    init_pair(11, COLOR_RED, COLOR_BLACK);   // 실패

    if (server_connection_successful()) {
        attron(COLOR_PAIR(10));
        mvprintw(y, x, "[서버 연결 성공]");
        attroff(COLOR_PAIR(10));
    } else {
        attron(COLOR_PAIR(11));
        mvprintw(y, x, "[서버 연결 실패]");
        attroff(COLOR_PAIR(11));
    }
}

// id_x, id_y, pw_y, pw_x 위치 구하기
void update_positions() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int logo_start_x = (max_x - strlen(art[0])) / 2;
    int logo_start_y = (max_y - art_lines - 8) / 2;
    int input_start_y = logo_start_y + art_lines + 3;
    id_y = input_start_y;
    id_x = logo_start_x + 18 + strlen("ID : ");
    pw_y = input_start_y + 2;
    pw_x = logo_start_x + 18 + strlen("PW : ");
}

// echo_mode 갱신
void update_echo_mode() {
    if (current_state == STATE_PW_INPUT) {
        noecho();
        curs_set(1);
    } else if (current_state == STATE_ID_INPUT) {
        echo();
        curs_set(1);
    } else {
        noecho();
        curs_set(0);
    }
}

// 커서 위치 업데이트
void update_cursor() {
    if (current_state == STATE_ID_INPUT)
        move(id_y, id_x + strlen(id_buf));
    else if (current_state == STATE_PW_INPUT)
        move(pw_y, pw_x + strlen(pw_buf));
}

// 로그인 화면 그리기
void draw_login_screen() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);

    update_positions();

    // 로고 그리기
    int logo_start_x = (max_x - strlen(art[0])) / 2;
    int logo_start_y = (max_y - art_lines - 8) / 2;
    for (int i = 0; i < art_lines; i++)
        mvprintw(logo_start_y + i, logo_start_x, "%s", art[i]);

    // ID 입력 그리기
    int input_start_y = logo_start_y + art_lines + 3;
    mvprintw(input_start_y, logo_start_x+18, "ID : ");
    mvprintw(input_start_y, id_x, "%-*s", MAX_INPUT, id_buf);

    // PW 입력 그리기
    mvprintw(input_start_y + 2, logo_start_x+18, "PW : ");
    mvprintw(input_start_y + 2, pw_x, "%-*s", MAX_INPUT, "");
    for (int i = 0; i < strlen(pw_buf); i++) {
        mvprintw(input_start_y + 2, pw_x+i, "*");
    }

    // Login 실패 메시지 
    if (!login_success) {
        attron(COLOR_PAIR(1)); // 빨간색
        mvprintw(input_start_y + 3, logo_start_x+18 + 5, "Invalid username or password");
        attroff(COLOR_PAIR(1));
    }

    // Login
    if (current_state == STATE_LOGIN_BUTTON) attron(A_REVERSE);
    mvprintw(input_start_y + 8, (max_x - 12) / 2 - 13, "Login");
    if (current_state == STATE_LOGIN_BUTTON) attroff(A_REVERSE);

    // Sign in
    if (current_state == STATE_SIGNUP_BUTTON) attron(A_REVERSE);
    mvprintw(input_start_y + 8, (max_x - 12) / 2 + 13, "Sign in");
    if (current_state == STATE_SIGNUP_BUTTON) attroff(A_REVERSE);

    draw_connection_status(max_y - 2, (max_x - 20) / 2); 
    refresh();
}

// 입력 처리
void handle_input(int ch) {
    UIState prev_state = current_state;
    switch (ch) {
        case KEY_UP:
            if (current_state == STATE_PW_INPUT) current_state = STATE_ID_INPUT;
            else if (current_state == STATE_LOGIN_BUTTON) current_state = STATE_PW_INPUT;
            else if (current_state == STATE_SIGNUP_BUTTON) current_state = STATE_PW_INPUT;
            break;
        case KEY_DOWN:
            if (current_state == STATE_ID_INPUT) current_state = STATE_PW_INPUT;
            else if (current_state == STATE_PW_INPUT) current_state = STATE_LOGIN_BUTTON;
            break;
        case KEY_LEFT:
            if (current_state == STATE_SIGNUP_BUTTON) current_state = STATE_LOGIN_BUTTON;
            break;
        case KEY_RIGHT:
            if (current_state == STATE_LOGIN_BUTTON) current_state = STATE_SIGNUP_BUTTON;
            break;
        case 10: // Enter
            if (current_state == STATE_LOGIN_BUTTON) {
                // 로그인 로직
                if(strcmp(id_buf, USER_ID) == 0 && strcmp(pw_buf, USER_PW) == 0) {
                    login_success = true;
                }

            } else if (current_state == STATE_SIGNUP_BUTTON) {
                // 회원가입 로직
            }
            break;
        case 127: // 백스페이스 눌렀을 때
        case KEY_BACKSPACE:
            if (current_state == STATE_ID_INPUT && strlen(id_buf) > 0) {
                int id_len = strlen(id_buf);
                id_buf[id_len - 1] = '\0';
                mvaddch(id_y, id_x + id_len - 1, ' ');
                move(id_y, id_x + id_len - 1);
            }
            else if (current_state == STATE_PW_INPUT && strlen(pw_buf) > 0) {
                int pw_len = strlen(pw_buf);
                pw_buf[pw_len - 1] = '\0';
                mvaddch(pw_y, pw_x + pw_len - 1, ' ');
                move(pw_y, pw_x + pw_len - 1);
            }
            draw_login_screen();
            break;
        default: // 문자 입력
            if (ch == 27) { // ESC 키 -> 다음 두 문자 읽어서 버림 (예: [C, [D, [A 등 처리)
                getch();
                getch();
                break;
            }
            if (ch >= 32 && ch <= 126) {
                if (current_state == STATE_ID_INPUT && strlen(id_buf) < MAX_INPUT - 1) {
                    strncat(id_buf, (char*)&ch, 1);
                    mvaddch(id_y, id_x + strlen(id_buf) - 1, ch);
                    move(id_y, id_x + strlen(id_buf));
                }
                else if (current_state == STATE_PW_INPUT && strlen(pw_buf) < MAX_INPUT - 1) {
                    strncat(pw_buf, (char*)&ch, 1);
                    int len = strlen(pw_buf);
                    mvaddch(pw_y, pw_x + len - 1, '*');
                    move(pw_y, pw_x + len);
                }
            }
            break;
    }

    // 상태가 바뀌었으면 화면 다시 그리기
    if (prev_state != current_state) {
        draw_login_screen();
    }
    update_cursor();
    update_echo_mode();
}

// 터미널 창 크기 바꼈을 때
void sigwinch_handler(int signo) {
    endwin();
    refresh();
    clear();
    resize_term(0, 0);
    draw_border();
    draw_login_screen();
    update_cursor();
    noecho(); 
    flushinp();
}

// 로그인 전체
void login_screen(char *id, char* pw) {
    // ncurses 초기화 및 환경 설정
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    start_color();
    signal(SIGWINCH, sigwinch_handler);

    // GUI 표시 전 서버 연결 시도
    connect_to_server("127.0.0.1", 4000);  // 연결 성공 여부는 내부에서 저장됨

    // 위치 계산 및 초기 출력
    update_positions();
    draw_border();
    draw_login_screen();  // draw_connection_status() 포함됨
    move(id_y, id_x);

    // 로그인 입력 처리 루프
    while (!login_success) {
        int ch = getch();
        if (ch == 'q') break;
        handle_input(ch);
    }

    endwin();  // 로그인 완료 시 ncurses 종료

    // ID/PW 결과 저장
    strncpy(id, id_buf, MAX_INPUT);
    strncpy(pw, pw_buf, MAX_INPUT);

    // 연결 성공했으면 대기화면 후 게임 실행
    if (server_connection_successful()) {
        // waiting(wait_for_match); // 진행 바 출력
        // system("./codenames");
        UserInfo user;
        strncpy(user.nickname, id_buf, sizeof(user.nickname));
        user.wins = 7;
        user.losses = 3;
        
        lobby_screen(&user);
    } else {
        printf("❌ 서버 연결 실패 (게임 실행 중단)\n");
    }
}

int main() {
    setlocale(LC_ALL, "");

    char id[MAX_INPUT] = {0};
    char pw[MAX_INPUT] = {0};
    login_screen(id, pw);
    printf("ID 입력 결과: %s\n", id);
    printf("PW 입력 결과: %s\n", pw);
    return 0;
}
