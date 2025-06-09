#include "client.h"
#include "login_screen.h"
#include "signup_screen.h"
#include "gui_utils.h"
#include <ncurses.h>
#include <string.h>
#include <signal.h>
#include <openssl/ssl.h>

#define USER_MAX_INPUT 32

typedef enum {
    LOGIN_STATE_ID_INPUT,
    LOGIN_STATE_PW_INPUT,
    LOGIN_STATE_LOGIN_BUTTON,
    LOGIN_STATE_SIGNUP_BUTTON
} LoginUIState;

typedef enum {
    LOGIN_NONE = -1,        // 초기값 또는 아직 로그인 시도 전
    LOGIN_SUCCESS = 0,      // 로그인 성공
    LOGIN_ERROR = 1,        // 일반 오류 (DB, 네트워크 등)
    LOGIN_NO_ACCOUNT = 2,   // 계정 없음
    LOGIN_WRONG_PW = 3,     // 비밀번호 불일치
    LOGIN_SUSPENDED = 4     // 정지된 계정
} LoginResult;

static char id_buf[USER_MAX_INPUT] = {0};
static char pw_buf[USER_MAX_INPUT] = {0};
static int id_y, id_x;
static int pw_y, pw_x;
static LoginUIState current_state = LOGIN_STATE_ID_INPUT;
static LoginResult login_result = LOGIN_NONE;
static int login_requested = 0;
static int show_message = 0;
static int id_valid = 0;
static int password_valid = 0;
static SceneState scene_state = SCENE_LOGIN;

// id_x, id_y, pw_y, pw_x 위치 구하기
static void update_positions() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int logo_start_x = (max_x - strlen(art[0])) / 2;
    int logo_start_y = (max_y - art_lines - 8) / 2;
    int input_start_y = logo_start_y + art_lines + 3;
    id_y = input_start_y;
    id_x = logo_start_x + 18 + strlen("ID : ");
    pw_y = input_start_y + 3;
    pw_x = logo_start_x + 18 + strlen("PW : ");
}

// echo_mode 갱신
static void update_echo_mode() {
    if (current_state == LOGIN_STATE_PW_INPUT) {
        noecho();
        curs_set(1);
    } else if (current_state == LOGIN_STATE_ID_INPUT) {
        echo();
        curs_set(1);
    } else {
        noecho();
        curs_set(0);
    }
}

// 커서 위치 업데이트
static void update_cursor() {
    if (current_state == LOGIN_STATE_ID_INPUT)
        move(id_y, id_x + strlen(id_buf));
    else if (current_state == LOGIN_STATE_PW_INPUT)
        move(pw_y, pw_x + strlen(pw_buf));
}

// 로그인 화면 그리기
static void draw_login_screen() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);

    update_positions();

    // 로고 그리기
    int logo_start_x = (max_x - strlen(art[0])) / 2;
    int logo_start_y = (max_y - art_lines - 8) / 2;
    for (int i = 0; i < art_lines; i++)
        mvprintw(logo_start_y + i, logo_start_x, "%s", art[i]);

    // ID 입력 그리기
    int input_start_y = logo_start_y + art_lines + 3;
    mvprintw(input_start_y, logo_start_x+18, "ID : ");
    mvprintw(input_start_y, id_x, "%-*s", USER_MAX_INPUT, id_buf);

    // ID 입력 안됨 문구 출력
    if(show_message == 1) {
        if (login_requested == 1 && id_buf[0] == '\0') {
            attron(COLOR_PAIR(1)); // 빨간색
            mvprintw(input_start_y+1, logo_start_x+18+5, "Please enter your ID.");
            attroff(COLOR_PAIR(1));
        }
    }

    // PW 입력 그리기
    mvprintw(input_start_y + 3, logo_start_x+18, "PW : ");
    mvprintw(input_start_y + 3, pw_x, "%-*s", USER_MAX_INPUT, "");
    for (int i = 0; i < strlen(pw_buf); i++) {
        mvprintw(input_start_y + 3, pw_x+i, "*");
    }

    static int  msg_len = 30;
    // PW 입력 안됨 문구 출력
    if(show_message == 1) {
        if (login_requested == 1 && pw_buf[0] == '\0') {
            attron(COLOR_PAIR(1)); 
            mvprintw(input_start_y + 4, logo_start_x+18+strlen("PW : "), "Please enter your password.");
            attroff(COLOR_PAIR(1));
        } else {
            mvprintw(input_start_y + 4, logo_start_x+18+strlen("PW : "), "%*s", msg_len, "");
        }
    }

    // 로그인 결과 문구 출력
    if (login_result == 0) { // 로그인 성공
        mvprintw(input_start_y + 6, logo_start_x+18 + 5, "%*s", 30, ""); // 이전 메시지 지우기
        attron(COLOR_PAIR(2)); // 초록색
        mvprintw(input_start_y + 6, (max_x - strlen("Login Successful")) / 2, "Login Successful");
        attroff(COLOR_PAIR(2));
        mvprintw(input_start_y+1, logo_start_x+18+5, "%-*s", USER_MAX_INPUT, "");
        mvprintw(input_start_y + 4, logo_start_x+18+strlen("PW : "), "%-*s", USER_MAX_INPUT, "");
    } else if (login_result == LOGIN_ERROR || login_result == LOGIN_NO_ACCOUNT || login_result == LOGIN_WRONG_PW) { // 로그인 실패
        mvprintw(input_start_y + 6, logo_start_x+18 + 5, "%*s", 30, ""); // 이전 메시지 지우기
        attron(COLOR_PAIR(1)); // 빨간색
        mvprintw(input_start_y + 6, logo_start_x+18 + 5, "Invalid username or password");
        attroff(COLOR_PAIR(1));
        mvprintw(input_start_y+1, logo_start_x+18+5, "%-*s", USER_MAX_INPUT, "");
        mvprintw(input_start_y + 4, logo_start_x+18+strlen("PW : "), "%-*s", USER_MAX_INPUT, "");
    } else if (login_result == LOGIN_SUSPENDED) { // 정지된 계정
        mvprintw(input_start_y + 6, logo_start_x+18 + 5, "%*s", 30, ""); // 이전 메시지 지우기
        attron(COLOR_PAIR(1)); // 빨간색
        mvprintw(input_start_y + 6, logo_start_x+18 + 5, "Account has been suspended");
        attroff(COLOR_PAIR(1));
        mvprintw(input_start_y+1, logo_start_x+18+5, "%-*s", USER_MAX_INPUT, "");
        mvprintw(input_start_y + 4, logo_start_x+18+strlen("PW : "), "%-*s", USER_MAX_INPUT, "");
    }

    // Login 버튼
    if (current_state == LOGIN_STATE_LOGIN_BUTTON) attron(A_REVERSE);
    mvprintw(input_start_y + 9, (max_x - 12) / 2 - 13, "Login");
    if (current_state == LOGIN_STATE_LOGIN_BUTTON) attroff(A_REVERSE);

    // Sign in 버튼
    if (current_state == LOGIN_STATE_SIGNUP_BUTTON) attron(A_REVERSE);
    mvprintw(input_start_y + 9, (max_x - 12) / 2 + 13, "Sign in");
    if (current_state == LOGIN_STATE_SIGNUP_BUTTON) attroff(A_REVERSE);

    refresh();
}

// 로그인 요청
LoginResult login_user(const char *id, const char *pw, char *token_buf, int token_buf_len) {
    // 요청 보냄
    char request[256];
    snprintf(request, sizeof(request), "LOGIN|%s|%s", id, pw);
    SSL_write(ssl, request, strlen(request));

    // 응답 받음
    char response[TOKEN_LEN + 16] = {0};
    int len = SSL_read(ssl, response, sizeof(response) - 1);
    if (len <= 0) {
        return LOGIN_ERROR;
    }
    response[len] = '\0';

    // 응답 파싱 (예: "LOGIN_OK|토큰값")
    if (strncmp(response, "LOGIN_OK|", 9) == 0) { // 로그인 성공
        if (token_buf && token_buf_len >= TOKEN_LEN) {
            strncpy(token_buf, response + 9, TOKEN_LEN - 1);
            token_buf[TOKEN_LEN - 1] = '\0';
        }
        return LOGIN_SUCCESS;
    } else if (strcmp(response, "LOGIN_NO_ACCOUNT") == 0) { // 로그인 실패
        return LOGIN_NO_ACCOUNT;
    } else if (strcmp(response, "LOGIN_WRONG_PW") == 0) {
        return LOGIN_WRONG_PW;
    } else if (strcmp(response, "LOGIN_SUSPENDED") == 0) { // 정지된 계정
        return LOGIN_SUSPENDED;
    } else {
        return LOGIN_ERROR;
    }
}


// 입력 처리
static SceneState handle_input(int ch) {
    LoginUIState prev_state = current_state;
    switch (ch) {
        case KEY_UP:
            if (current_state == LOGIN_STATE_PW_INPUT) current_state = LOGIN_STATE_ID_INPUT;
            else if (current_state == LOGIN_STATE_LOGIN_BUTTON) current_state = LOGIN_STATE_PW_INPUT;
            else if (current_state == LOGIN_STATE_SIGNUP_BUTTON) current_state = LOGIN_STATE_PW_INPUT;
            break;
        case KEY_DOWN:
            if (current_state == LOGIN_STATE_ID_INPUT) current_state = LOGIN_STATE_PW_INPUT;
            else if (current_state == LOGIN_STATE_PW_INPUT) current_state = LOGIN_STATE_LOGIN_BUTTON;
            break;
        case KEY_LEFT:
            if (current_state == LOGIN_STATE_SIGNUP_BUTTON) current_state = LOGIN_STATE_LOGIN_BUTTON;
            break;
        case KEY_RIGHT:
            if (current_state == LOGIN_STATE_LOGIN_BUTTON) current_state = LOGIN_STATE_SIGNUP_BUTTON;
            break;
        case 10: // Enter
            if (current_state == LOGIN_STATE_LOGIN_BUTTON) { // 로그인
                login_requested = 1;
                show_message = 1;
                // 아이디가 1글자 이상이면 유효
                if (strlen(id_buf) > 0) {
                    id_valid = 1;
                } else {
                    id_valid = 0;
                }
                // 비밀번호가 1글자 이상이면 유효
                if (strlen(pw_buf) > 0) {
                    password_valid = 1;
                } else {
                    password_valid = 0;
                }
                // 로그인 조건 불충족 -> 문구 출력
                if (id_valid != 1 || password_valid != 1) {
                    draw_login_screen();
                } else { // 로그인 시도
                    login_result = login_user(id_buf, pw_buf, token, TOKEN_LEN);
                    show_message = 0;
                    draw_login_screen();
                    if (login_result == LOGIN_SUCCESS) {
                        return SCENE_MAIN;
                    }
                }
            } else if (current_state == LOGIN_STATE_SIGNUP_BUTTON) { // 회원가입 페이지로 이동
                return SCENE_SIGNUP;
            }
            break;
        case 127: // 백스페이스 눌렀을 때
        case KEY_BACKSPACE:
            if (current_state == LOGIN_STATE_ID_INPUT && strlen(id_buf) > 0) {
                int id_len = strlen(id_buf);
                id_buf[id_len - 1] = '\0';
                mvaddch(id_y, id_x + id_len - 1, ' ');
                move(id_y, id_x + id_len - 1);
            }
            else if (current_state == LOGIN_STATE_PW_INPUT && strlen(pw_buf) > 0) {
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
                if (current_state == LOGIN_STATE_ID_INPUT && strlen(id_buf) < MAX_INPUT - 1) {
                    strncat(id_buf, (char*)&ch, 1);
                    mvaddch(id_y, id_x + strlen(id_buf) - 1, ch);
                    move(id_y, id_x + strlen(id_buf));
                }
                else if (current_state == LOGIN_STATE_PW_INPUT && strlen(pw_buf) < MAX_INPUT - 1) {
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
        show_message = 0;
        draw_login_screen();
    }
    update_cursor();
    update_echo_mode();
    return SCENE_LOGIN;
}

// 터미널 창 크기 바꼈을 때
static void sigwinch_handler(int signo) {
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
SceneState login_screen(char *id, char* pw) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    signal(SIGWINCH, sigwinch_handler);

    update_positions();
    draw_border();
    draw_login_screen();
    move(id_y, id_x);

    while (1) {
        int ch = getch();
        if (ch == 'q') break;
        scene_state = handle_input(ch);
        if(scene_state != SCENE_LOGIN)
            break;
    }
    endwin();

    strncpy(id, id_buf, MAX_INPUT);
    strncpy(pw, pw_buf, MAX_INPUT);

    return scene_state;
}
