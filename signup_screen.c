#include <ncurses.h>
#include <string.h>
#include <signal.h>

#define MAX_INPUT 32

typedef enum {
    STATE_ID_INPUT,
    STATE_PW_INPUT,
    STATE_NICKNAME_INPUT,
    STATE_ID_CHECK,
    STATE_NICKNAME_CHECK,
    STATE_SIGNUP_BUTTON
} UIState;

static char id_buf[MAX_INPUT] = {0};
static char pw_buf[MAX_INPUT] = {0};
static char nickname_buf[MAX_INPUT] = {0};
static int id_y, id_x;
static int pw_y, pw_x;
static int nickname_y, nickname_x;
UIState current_state = STATE_ID_INPUT;


const char* art[] = {
    ":::::::  :::::::  :::::    :::::::      ::   ::    ::     ::: :::  :::::::  :::::::",
    "+:       +:   :+  +:  :+   +:           +:+  :+   +: :+   +:: :+:  +:       +:     ",
    "#+       #+   +#  #+   +#  #+#+#+#      #+#+ +#  #+#+#+#  #+ + #+  #+#+#+#  #+#+#+#",
    "#+       #+   +#  #+  +#   #+           #+ +#+#  #+   +#  #+   #+  #+            +#",
    "#######  #######  #####    #######      ##   ##  ##   ##  ##   ##  #######  #######"
};

// 코드 네임즈 로고가 5줄로 이루어져 있음
const int art_lines = 5;

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
}

// 각각의 좌표 위치 업데이트
void update_positions() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int logo_start_x = (max_x - strlen(art[0])) / 2;
    int logo_start_y = (max_y - art_lines - 8) / 4;
    int input_start_y = max_y/7*3;

    id_y = input_start_y;
    id_x = logo_start_x + 18 + strlen("ID : ");

    pw_y = input_start_y + 5;
    pw_x = logo_start_x + 18 + strlen("PW : ");

    nickname_y = input_start_y + 9;
    nickname_x = logo_start_x + 18 + strlen("Nickname : ");
}

// echo_mode 갱신
void update_echo_mode() {
    if (current_state == STATE_PW_INPUT) {
        noecho();
        curs_set(1);
    } else if (current_state == STATE_ID_INPUT || current_state == STATE_NICKNAME_INPUT) {
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
    else if (current_state == STATE_NICKNAME_INPUT)
        move(nickname_y, nickname_x + strlen(nickname_buf));      
}

// 화면 그리기
void draw_signup_screen() {
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
    mvprintw(input_start_y, logo_start_x+18+strlen("ID : "), "%-*s", MAX_INPUT, id_buf);

    if (current_state == STATE_ID_CHECK) attron(A_REVERSE);
    mvprintw(input_start_y, logo_start_x+65, "%s", "[Check Availability]");
    if (current_state == STATE_ID_CHECK) attroff(A_REVERSE);

    attron(COLOR_PAIR(1)); // 빨간색
    mvprintw(input_start_y + 1, logo_start_x+18 + 5, "Sorry, this ID is already taken.");
    attroff(COLOR_PAIR(1));

    // PW 출력
    mvprintw(input_start_y + 5, logo_start_x+18, "PW : ");
    mvprintw(input_start_y + 5, pw_x, "%-*s", MAX_INPUT, "");
    for (int i = 0; i < strlen(pw_buf); i++) {
        mvprintw(input_start_y + 5, pw_x+i, "*");
    }

    // 닉네임 출력
    mvprintw(input_start_y + 9, logo_start_x+18, "Nickname : ");
    mvprintw(input_start_y + 9, logo_start_x+18+strlen("Nickname : "), "%-*s", MAX_INPUT, nickname_buf);

    if (current_state == STATE_NICKNAME_CHECK) attron(A_REVERSE);
    mvprintw(input_start_y + 9, logo_start_x+58, "%s", "[Check Availability]");
    if (current_state == STATE_NICKNAME_CHECK) attroff(A_REVERSE);

    // 사용 가능 메시지
    attron(COLOR_PAIR(2)); // 초록색
    mvprintw(input_start_y + 10, logo_start_x+18 + 11, "This username is available!");
    attroff(COLOR_PAIR(2));

    // Sign up 버튼
    if (current_state == STATE_SIGNUP_BUTTON) attron(A_REVERSE);
    mvprintw(input_start_y + 15, (max_x - 12) / 2, "Sign up");
    if (current_state == STATE_SIGNUP_BUTTON) attroff(A_REVERSE);

    refresh();
}

// 입력 처리
void handle_input(int ch) {
    UIState prev_state = current_state;
    switch (ch) {
        case KEY_UP:
            if (current_state == STATE_PW_INPUT) current_state = STATE_ID_INPUT;
            else if (current_state == STATE_NICKNAME_INPUT) current_state = STATE_PW_INPUT;
            else if (current_state == STATE_SIGNUP_BUTTON) current_state = STATE_NICKNAME_INPUT;
            break;
        case KEY_DOWN:
            if (current_state == STATE_ID_INPUT) current_state = STATE_PW_INPUT;
            else if (current_state == STATE_ID_CHECK) current_state = STATE_PW_INPUT;
            else if (current_state == STATE_PW_INPUT) current_state = STATE_NICKNAME_INPUT;
            else if (current_state == STATE_NICKNAME_INPUT) current_state = STATE_SIGNUP_BUTTON;
            else if (current_state == STATE_NICKNAME_CHECK) current_state = STATE_SIGNUP_BUTTON;
            break;
        case KEY_LEFT:
            if (current_state == STATE_ID_CHECK) current_state = STATE_ID_INPUT;
            else if (current_state == STATE_NICKNAME_CHECK) current_state = STATE_NICKNAME_INPUT;
            break;
        case KEY_RIGHT:
            if (current_state == STATE_ID_INPUT) current_state = STATE_ID_CHECK;
            else if (current_state == STATE_NICKNAME_INPUT) current_state = STATE_NICKNAME_CHECK;
            break;
        case 10: // Enter
            if (current_state == STATE_ID_CHECK) {
                // 아이디 체크 로직
            }
            else if (current_state == STATE_NICKNAME_CHECK) {
                // 닉네임 체크 로직
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
            if (current_state == STATE_NICKNAME_INPUT && strlen(nickname_buf) > 0) {
                int nick_len = strlen(nickname_buf);
                nickname_buf[nick_len - 1] = '\0';
                mvaddch(nickname_y, nickname_x + nick_len - 1, ' ');
                move(nickname_y, nickname_x + nick_len - 1);
            }
            else if (current_state == STATE_PW_INPUT && strlen(pw_buf) > 0) {
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
                if (current_state == STATE_ID_INPUT && strlen(id_buf) < MAX_INPUT - 1) {
                    strncat(id_buf, (char*)&ch, 1);
                    mvaddch(id_y, id_x + strlen(id_buf) - 1, ch);
                    move(id_y, id_x + strlen(id_buf));
                }
                else if (current_state == STATE_NICKNAME_INPUT && strlen(nickname_buf) < MAX_INPUT - 1) {
                    strncat(nickname_buf, (char*)&ch, 1);
                    mvaddch(nickname_y, nickname_x + strlen(nickname_buf) - 1, ch);
                    move(nickname_y, nickname_x + strlen(id_buf));
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
        draw_signup_screen();
    }
    update_cursor();
    update_echo_mode();
}

// 터미널 사이즈 바꼈을 때
void sigwinch_handler(int signo) {
    endwin();
    refresh();
    clear();
    resize_term(0, 0);
    draw_border();
    draw_signup_screen();
    update_cursor();
    noecho(); 
    flushinp();
}

// 로그인 전체
void signup_screen(char *id, char* pw, char* nickname) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    signal(SIGWINCH, sigwinch_handler);

    update_positions();
    draw_border();
    draw_signup_screen();
    move(id_y, id_x);

    while (1) {
        int ch = getch();
        if (ch == 'q') break;
        handle_input(ch);
    }
    endwin();

    strncpy(id, id_buf, MAX_INPUT);
    strncpy(pw, pw_buf, MAX_INPUT);
    strncpy(nickname, nickname_buf, MAX_INPUT);
}

int main() {
    char id[MAX_INPUT] = {0};
    char pw[MAX_INPUT] = {0};
    char nick[MAX_INPUT] = {0};
    signup_screen(id, pw, nick);
    printf("ID 입력 결과: %s\n", id);
    printf("PW 입력 결과: %s\n", pw);
    printf("NICK 입력 결과: %s\n", nick);
    return 0;
}

