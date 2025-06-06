#include "client.h"
#include "signup_screen.h"
#include "gui_utils.h"
#include <ncurses.h>
#include <string.h>
#include <signal.h>

#define SIGN_MAX_INPUT 32

typedef enum {
    SIGNUP_STATE_ID_INPUT,
    SIGNUP_STATE_PW_INPUT,
    SIGNUP_STATE_NICKNAME_INPUT,
    SIGNUP_STATE_ID_CHECK,
    SIGNUP_STATE_NICKNAME_CHECK,
    SIGNUP_STATE_SIGNUP_BUTTON
} SignupUIState;

// 중복 체크 결과
typedef enum {
    AVAILABLE = 0,     // 사용 가능
    DUPLICATED = -1,   // 중복
    ERROR = -2,         // 에러
    NOT_CHECKED = -3   // 아직 체크하지 않음
} SignupCheckResult;

// 회원 가입 결과
typedef enum {
    SIGNUP_NONE = -1,
    SIGNUP_SUCCESS = 0,    // 회원가입 성공
    SIGNUP_ERROR = 1,      // 에러 (네트워크, 서버 등)
} SignupResult;

static char id_buf[SIGN_MAX_INPUT] = {0};
static char pw_buf[SIGN_MAX_INPUT] = {0};
static char nickname_buf[SIGN_MAX_INPUT] = {0};
static int id_y, id_x;
static int pw_y, pw_x;
static int nickname_y, nickname_x;
static SignupUIState current_state = SIGNUP_STATE_ID_INPUT;
static SignupCheckResult id_available = NOT_CHECKED;
static SignupCheckResult nickname_available = NOT_CHECKED;
static int id_valid = 0;
static int id_valid_request = 0;
static int password_valid = 0;
static int nickname_valid = 0;
static int nickname_valid_request = 0;
static int signup_requested = 0;
static int show_message = 0;
static SignupResult signup_result = SIGNUP_NONE;
static SceneState scene_state = SCENE_SIGNUP;

// 각각의 좌표 위치 업데이트
static void update_positions() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int logo_start_x = (max_x - strlen(art[0])) / 2;
    int input_start_y = max_y/7*3;

    id_y = input_start_y;
    id_x = logo_start_x + 18 + strlen("ID : ");

    pw_y = input_start_y + 5;
    pw_x = logo_start_x + 18 + strlen("PW : ");

    nickname_y = input_start_y + 9;
    nickname_x = logo_start_x + 18 + strlen("Nickname : ");
}

// echo_mode 갱신
static void update_echo_mode() {
    if (current_state == SIGNUP_STATE_PW_INPUT) {
        noecho();
        curs_set(1);
    } else if (current_state == SIGNUP_STATE_ID_INPUT || current_state == SIGNUP_STATE_NICKNAME_INPUT) {
        echo();
        curs_set(1);
    } else {
        noecho();
        curs_set(0);
    }
}

// 커서 위치 업데이트
static void update_cursor() {
    if (current_state == SIGNUP_STATE_ID_INPUT)
        move(id_y, id_x + strlen(id_buf));
    else if (current_state == SIGNUP_STATE_PW_INPUT)
        move(pw_y, pw_x + strlen(pw_buf));
    else if (current_state == SIGNUP_STATE_NICKNAME_INPUT)
        move(nickname_y, nickname_x + strlen(nickname_buf));      
}

// 화면 그리기
static void draw_signup_screen() {

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    update_positions();
    start_color(); // 색상 기능 활성화

    // 색상 쌍 설정
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);

    // 로고 출력
    int logo_start_x = (max_x - strlen(art[0])) / 2;
    int logo_start_y = (max_y - art_lines - 8) / 4;
    for (int i = 0; i < art_lines; i++) {
        mvprintw(logo_start_y + i, logo_start_x, "%s", art[i]);
    }

    // ID 출력
    int input_start_y = logo_start_y + art_lines + 3;
    input_start_y = max_y/7*3;    
    mvprintw(input_start_y, logo_start_x+18, "ID : ");
    mvprintw(input_start_y, logo_start_x+18+strlen("ID : "), "%-*s", SIGN_MAX_INPUT, id_buf);

    if (current_state == SIGNUP_STATE_ID_CHECK) attron(A_REVERSE);
    mvprintw(input_start_y, logo_start_x+65, "%s", "[Check Availability]");
    if (current_state == SIGNUP_STATE_ID_CHECK) attroff(A_REVERSE);

    int msg_len = 40;

    if(show_message == 1) {
        if (signup_requested == 1 && id_valid == 0 && id_valid_request == 0) { // ID 입력 안됨
            attron(COLOR_PAIR(1)); // 빨간색
            mvprintw(input_start_y + 1, logo_start_x+18 + 5, "Please enter your ID.");
            attroff(COLOR_PAIR(1));
        } else if (id_available == AVAILABLE) { // ID 사용 가능
            mvprintw(input_start_y + 1, logo_start_x+18 + 5, "%*s", msg_len, "");
            attron(COLOR_PAIR(2));
            mvprintw(input_start_y + 1, logo_start_x+18 + 5, "This ID is available!");
            attroff(COLOR_PAIR(2));
        } else if (id_available == DUPLICATED) { // ID 중복
            mvprintw(input_start_y + 1, logo_start_x+18 + 5, "%*s", msg_len, "");
            attron(COLOR_PAIR(1)); // 빨간색
            mvprintw(input_start_y + 1, logo_start_x+18 + 5, "Sorry, this ID is already taken.");
            attroff(COLOR_PAIR(1));
        } else if (signup_requested == 1 && id_available == NOT_CHECKED) { // ID 중복 체크 확인
            attron(COLOR_PAIR(1));
            mvprintw(input_start_y + 1, logo_start_x+18 + 5, "Please check ID availability.");
            attroff(COLOR_PAIR(1));
        } else if (id_available == ERROR) { // ID 에러
            mvprintw(input_start_y + 1, logo_start_x+18 + 5, "%*s", msg_len, "");
            attron(COLOR_PAIR(1));
            mvprintw(input_start_y + 1, logo_start_x+18 + 5, "Error checking ID availability.");
            attroff(COLOR_PAIR(1));
        }
    }

    // PW 출력
    mvprintw(input_start_y + 5, logo_start_x+18, "PW : ");
    mvprintw(input_start_y + 5, pw_x, "%-*s", SIGN_MAX_INPUT, "");
    for (int i = 0; i < strlen(pw_buf); i++) {
        mvprintw(input_start_y + 5, pw_x+i, "*");
    }

    if(show_message == 1) {
        if (signup_requested == 1 && password_valid == 0) { // PW 입력 안됨
            attron(COLOR_PAIR(1)); 
            mvprintw(input_start_y + 6, logo_start_x+18+strlen("PW : "), "Please enter your password.");
            attroff(COLOR_PAIR(1));
        } else {
            mvprintw(input_start_y + 6, logo_start_x+18+strlen("PW : "), "%*s", msg_len, "");
        }
    }

    // 닉네임 출력
    mvprintw(input_start_y + 9, logo_start_x+18, "Nickname : ");
    mvprintw(input_start_y + 9, logo_start_x+18+strlen("Nickname : "), "%-*s", SIGN_MAX_INPUT, nickname_buf);

    if (current_state == SIGNUP_STATE_NICKNAME_CHECK) attron(A_REVERSE);
    mvprintw(input_start_y + 9, logo_start_x+58, "%s", "[Check Availability]");
    if (current_state == SIGNUP_STATE_NICKNAME_CHECK) attroff(A_REVERSE);

    if(show_message == 1) {
        if (signup_requested == 1 && nickname_valid == 0 && nickname_valid_request == 0) { // 닉네임 입력 안 됨
            attron(COLOR_PAIR(1)); 
            mvprintw(input_start_y + 10, logo_start_x+18 + 11, "Please enter your nickname.");
            attroff(COLOR_PAIR(1));
        } else if (nickname_available == AVAILABLE) { // 닉네임 사용 가능
            mvprintw(input_start_y + 10, logo_start_x+18 + 11, "%*s", msg_len, "");
            attron(COLOR_PAIR(2));
            mvprintw(input_start_y + 10, logo_start_x+18 + 11, "This nickname is available!");
            attroff(COLOR_PAIR(2));
        } else if (nickname_available == DUPLICATED) { // 닉네임 중복
            mvprintw(input_start_y + 10, logo_start_x+18 + 11, "%*s", msg_len, "");
            attron(COLOR_PAIR(1)); // 빨간색
            mvprintw(input_start_y + 10, logo_start_x+18 + 11, "Sorry, this nickname is already taken.");
            attroff(COLOR_PAIR(1));
        } else if (signup_requested == 1 && nickname_available == NOT_CHECKED) { // 닉네임 중복 체크 확인
            attron(COLOR_PAIR(1));
            mvprintw(input_start_y + 10, logo_start_x+18 + 11, "Please check nickname availability.");
            attroff(COLOR_PAIR(1));
        } else if (nickname_available == ERROR) { // 닉네임 에러
            mvprintw(input_start_y + 10, logo_start_x+18 + 11, "%*s", msg_len, "");
            attron(COLOR_PAIR(1));
            mvprintw(input_start_y + 10, logo_start_x+18 + 11, "Error checking nickname availability.");
            attroff(COLOR_PAIR(1));
        }
    }

    // 회원 가입 결과 문구 출력
    /*if (signup_result == SIGNUP_SUCCESS) {
        mvprintw(input_start_y + 12, (max_x - strlen("Registration complete!"))/2, "%*s", 30, ""); // 이전 메시지 지우기
        attron(COLOR_PAIR(2)); // 초록색
        mvprintw(input_start_y + 12, (max_x - strlen("Registration complete!"))/2, "Registration complete!");
        attroff(COLOR_PAIR(2));
    } else*/ if (signup_result == SIGNUP_ERROR) {
        mvprintw(input_start_y + 12, (max_x - strlen("Signup failed."))/2, "%*s", 30, ""); // 이전 메시지 지우기
        attron(COLOR_PAIR(1)); // 빨간색
        mvprintw(input_start_y + 12, (max_x - strlen("Signup failed."))/2, "Signup failed.");
        attroff(COLOR_PAIR(1));
    }

    // Sign up 버튼
    if (current_state == SIGNUP_STATE_SIGNUP_BUTTON) attron(A_REVERSE);
    mvprintw(input_start_y + 15, (max_x - 12) / 2, "Sign up");
    if (current_state == SIGNUP_STATE_SIGNUP_BUTTON) attroff(A_REVERSE);

    refresh();
}

 // id 중복 확인
SignupCheckResult check_id_availability(const char *id) {
    if (ssl == NULL) { // SSL 연결 에러
        return ERROR;
    }

    char request[256];
    snprintf(request, sizeof(request), "CHECK_ID|%s", id);
    if (SSL_write(ssl, request, strlen(request)) <= 0) {
        perror("SSL_write");
        return ERROR;
    }

    char response[256] = {0};
    int recv_len = SSL_read(ssl, response, sizeof(response)-1);
    if (recv_len <= 0) {
        perror("SSL_read");
        return ERROR;
    }
    response[recv_len] = '\0';

    if (strncmp(response, "ID_AVAILABLE", 12) == 0) {
        return AVAILABLE;
    } else if (strncmp(response, "ID_TAKEN", 8) == 0) {
        return DUPLICATED;
    } else {
        return ERROR;
    }
}

// 닉네임 중복 확인
SignupCheckResult check_nickname_availability(const char *nickname) {
    if (ssl == NULL) {  // SSL 연결 에러
        return ERROR;
    }

    char request[256];
    snprintf(request, sizeof(request), "CHECK_NICKNAME|%s", nickname);
    if (SSL_write(ssl, request, strlen(request)) <= 0) {
        perror("SSL_write");
        return ERROR;
    }

    char response[256] = {0};
    int recv_len = SSL_read(ssl, response, sizeof(response)-1);
    if (recv_len <= 0) {
        perror("SSL_read");
        return ERROR;
    }
    response[recv_len] = '\0';

    if (strncmp(response, "NICKNAME_AVAILABLE", 18) == 0) {
        return AVAILABLE;
    } else if (strncmp(response, "NICKNAME_TAKEN", 14) == 0) {
        return DUPLICATED;
    } else {
        return ERROR;
    }
}

// 회원가입 요청
SignupResult send_signup_request(const char *id, const char *pw, const char *nickname) {
    char request[256];
    snprintf(request, sizeof(request), "SIGNUP|%s|%s|%s", id, pw, nickname);
    SSL_write(ssl, request, strlen(request));

    char response[TOKEN_LEN + 16] = {0};
    int len = SSL_read(ssl, response, sizeof(response) - 1);
    if (len <= 0) {
        return SIGNUP_ERROR;
    }
    response[len] = '\0';

    // 응답 파싱 (예: "SIGNUP_OK|토큰값")
    if (strncmp(response, "SIGNUP_OK|", 10) == 0) {
        strncpy(token, response + 10, TOKEN_LEN - 1); // 토큰 저장
        token[TOKEN_LEN - 1] = '\0';
        return SIGNUP_SUCCESS;
    } else {
        return SIGNUP_ERROR;
    }
}

// 입력 처리
static SceneState handle_input(int ch) {
    SignupUIState prev_state = current_state;
    switch (ch) {
        case KEY_UP:
            if (current_state == SIGNUP_STATE_PW_INPUT) current_state = SIGNUP_STATE_ID_INPUT;
            else if (current_state == SIGNUP_STATE_NICKNAME_INPUT) current_state = SIGNUP_STATE_PW_INPUT;
            else if (current_state == SIGNUP_STATE_SIGNUP_BUTTON) current_state = SIGNUP_STATE_NICKNAME_INPUT;
            break;
        case KEY_DOWN:
            if (current_state == SIGNUP_STATE_ID_INPUT) current_state = SIGNUP_STATE_PW_INPUT;
            else if (current_state == SIGNUP_STATE_ID_CHECK) current_state = SIGNUP_STATE_PW_INPUT;
            else if (current_state == SIGNUP_STATE_PW_INPUT) current_state = SIGNUP_STATE_NICKNAME_INPUT;
            else if (current_state == SIGNUP_STATE_NICKNAME_INPUT) current_state = SIGNUP_STATE_SIGNUP_BUTTON;
            else if (current_state == SIGNUP_STATE_NICKNAME_CHECK) current_state = SIGNUP_STATE_SIGNUP_BUTTON;
            break;
        case KEY_LEFT:
            if (current_state == SIGNUP_STATE_ID_CHECK) current_state = SIGNUP_STATE_ID_INPUT;
            else if (current_state == SIGNUP_STATE_NICKNAME_CHECK) current_state = SIGNUP_STATE_NICKNAME_INPUT;
            break;
        case KEY_RIGHT:
            if (current_state == SIGNUP_STATE_ID_INPUT) current_state = SIGNUP_STATE_ID_CHECK;
            else if (current_state == SIGNUP_STATE_NICKNAME_INPUT) current_state = SIGNUP_STATE_NICKNAME_CHECK;
            break;
        case 10: // Enter
            if (current_state == SIGNUP_STATE_ID_CHECK) { // 아이디 중복 체크
                id_valid_request = 1;
                id_available = check_id_availability(id_buf);
                show_message = 1;
                draw_signup_screen();
            }
            else if (current_state == SIGNUP_STATE_NICKNAME_CHECK) { // 닉네임 중복 체크
                nickname_valid_request = 1;
                nickname_available = check_nickname_availability(nickname_buf);
                show_message = 1;
                draw_signup_screen();
            } else if (current_state == SIGNUP_STATE_SIGNUP_BUTTON) { // 회원 가입
                signup_requested = 1;
                show_message = 1;
                if (strlen(id_buf) > 0) { // 비밀번호가 1글자 이상이면 유효
                    id_valid = 1;
                } else {
                    id_valid = 0;
                }
                if (strlen(pw_buf) > 0) { // 비밀번호가 1글자 이상이면 유효
                    password_valid = 1;
                } else {
                    password_valid = 0;
                }
                if (strlen(nickname_buf) > 0) { // 비밀번호가 1글자 이상이면 유효
                    nickname_valid = 1;
                } else {
                    nickname_valid = 0;
                }
                if (id_available != AVAILABLE || nickname_available != AVAILABLE || password_valid != 1) { // 회원가입 요건 불충족 -> 문구 출력
                    draw_signup_screen();
                }
                else if (id_available == AVAILABLE && nickname_available == AVAILABLE && password_valid == 1) { // 회원가입 시도
                    signup_result = send_signup_request(id_buf, pw_buf, nickname_buf);
                    draw_signup_screen();
                    if (signup_result == SIGNUP_SUCCESS) {
                        return SCENE_MAIN;    // 메인 화면으로 이동하는 로직
                    }
                }
            }
            break;
        case 127: // 백스페이스 눌렀을 때
        case KEY_BACKSPACE:
            if (current_state == SIGNUP_STATE_ID_INPUT && strlen(id_buf) > 0) {
                int id_len = strlen(id_buf);
                id_buf[id_len - 1] = '\0';
                mvaddch(id_y, id_x + id_len - 1, ' ');
                move(id_y, id_x + id_len - 1);
            }
            if (current_state == SIGNUP_STATE_NICKNAME_INPUT && strlen(nickname_buf) > 0) {
                int nick_len = strlen(nickname_buf);
                nickname_buf[nick_len - 1] = '\0';
                mvaddch(nickname_y, nickname_x + nick_len - 1, ' ');
                move(nickname_y, nickname_x + nick_len - 1);
            }
            else if (current_state == SIGNUP_STATE_PW_INPUT && strlen(pw_buf) > 0) {
                int pw_len = strlen(pw_buf);
                pw_buf[pw_len - 1] = '\0';
                mvaddch(pw_y, pw_x + pw_len - 1, ' ');
                move(pw_y, pw_x + pw_len - 1);
            }
            draw_signup_screen();
            break;
        default: // 문자 입력
            if (ch == 27) { // 화살표 처리 : ESC 키 -> 다음 두 문자 읽어서 버림 (예: [C, [D, [A 등 처리)
                getch();
                getch();
                printf("chs");
                break;
            }
            else if (ch >= 32 && ch <= 126) {
                if (current_state == SIGNUP_STATE_ID_INPUT && strlen(id_buf) < MAX_INPUT - 1) {
                    strncat(id_buf, (char*)&ch, 1);
                    mvaddch(id_y, id_x + strlen(id_buf) - 1, ch);
                    move(id_y, id_x + strlen(id_buf));
                }
                else if (current_state == SIGNUP_STATE_NICKNAME_INPUT && strlen(nickname_buf) < MAX_INPUT - 1) {
                    strncat(nickname_buf, (char*)&ch, 1);
                    mvaddch(nickname_y, nickname_x + strlen(nickname_buf) - 1, ch);
                    move(nickname_y, nickname_x + strlen(id_buf));
                }
                else if (current_state == SIGNUP_STATE_PW_INPUT && strlen(pw_buf) < MAX_INPUT - 1) {
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
        draw_signup_screen();
    }
    update_cursor();
    update_echo_mode();
    return SCENE_SIGNUP;
}

// 터미널 사이즈 바꼈을 때
static void sigwinch_handler(int signo) {
    endwin();
    refresh();
    erase();
    resize_term(0, 0);
    draw_border();
    draw_signup_screen();
    update_cursor();
    noecho(); 
    flushinp();
}

// 로그인 전체
SceneState signup_screen(char *id, char* pw, char* nickname) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    erase(); // 화면 한 번 지우기
    refresh(); // 화면 갱신

    signal(SIGWINCH, sigwinch_handler);

    update_positions();
    draw_border();
    draw_signup_screen();
    move(id_y, id_x);

    while (1) {
        int ch = getch();
        if (ch == 'q') break;
        scene_state = handle_input(ch);
        if(scene_state != SCENE_SIGNUP)
            break;
    }
    endwin();

    strncpy(id, id_buf, MAX_INPUT);
    strncpy(pw, pw_buf, MAX_INPUT);
    strncpy(nickname, nickname_buf, MAX_INPUT);

    return scene_state;
}

