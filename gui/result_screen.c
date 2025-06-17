#include "client.h"
#include "gui_utils.h"
#include "gui_invalid_token.h"
#include "codenames_screen.h"
#include <wchar.h>
#include <locale.h>
#include <ncursesw/ncurses.h>
#include <wctype.h>
#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
/*
typedef enum {
    RESULT_WIN,
    RESULT_LOSE,
} Result;
*/
volatile sig_atomic_t resized = 0;

void handle_winch(int sig) {
    resized = 1;
}

void register_sigwinch_handler() {
    struct sigaction sa;
    sa.sa_handler = handle_winch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);
}

static SceneState scene_state = SCENE_RESULT;

SceneState send_game_result(int sock, const char *token, Result result) {
    char buffer[256];
    if (result == RESULT_WIN) {
        snprintf(buffer, sizeof(buffer), "RESULT|%s|WIN", token);
    } else if (result == RESULT_LOSE) {
        snprintf(buffer, sizeof(buffer), "RESULT|%s|LOSS", token);
    } else {
        return SCENE_ERROR; // 잘못된 결과
    }
 
    send(sock, buffer, strlen(buffer), 0);

    ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        perror("recv failed or connection closed");
        return SCENE_ERROR;
    }
    buffer[received] = '\0';  // 널 종료자 추가

    // 유효하지 않은 토큰인 경우
    if (strcmp(buffer, "INVALID_TOKEN") == 0) {
    // fprintf(stderr, "유효하지 않은 토큰입니다.\n");
        close(game_sock);
        return SCENE_INVALID_TOKEN;
    }
    // 에러
    if (strcmp(buffer, "ERROR") == 0) {
        close(game_sock);
        return SCENE_ERROR;
    }

    // 받은 메시지 출력
    printf("Response from server: %s\n", buffer);
    return SCENE_MAIN;
}

// 결과 그리기
void draw_result(int red_score, int blue_score, Result result, int winner_team) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    setlocale(LC_ALL, "");  // 유니코드 출력 필수

    const wchar_t* msg;
    switch (result) {
        case RESULT_WIN: msg = L"Y O U   W I N !"; break;
        case RESULT_LOSE: msg = L"Y O U   L O S E !"; break;
        default: msg = L""; break;
    }

    int y1 = max_y / 8;
    int x1 = (max_x - wcswidth(msg, wcslen(msg))) / 2;
    attron(COLOR_PAIR(1)); // 노란색
    mvaddwstr(y1, x1, msg);
    attroff(COLOR_PAIR(1));

    wchar_t red_str[64];
    wchar_t blue_str[64];
    swprintf(red_str, sizeof(red_str)/sizeof(wchar_t), L"RED TEAM : %d", red_score);
    swprintf(blue_str, sizeof(blue_str)/sizeof(wchar_t), L"BLUE TEAM : %d", blue_score);

    const wchar_t* result_msg = L"";
    int result_y = max_y / 3;
    int result_x = 0;

    if (winner_team == 0) {
        result_msg = L"빨간 팀이 이겼습니다";
        result_x = (max_x - wcswidth(result_msg, wcslen(result_msg))) / 2;
        attron(COLOR_PAIR(2)); // 빨간색
        mvaddwstr(result_y, result_x, result_msg);
        attroff(COLOR_PAIR(2));
    } else if (winner_team == 1) {
        result_msg = L"파란 팀이 이겼습니다";
        result_x = (max_x - wcswidth(result_msg, wcslen(result_msg))) / 2;
        attron(COLOR_PAIR(3)); // 파란색
        mvaddwstr(result_y, result_x, result_msg);
        attroff(COLOR_PAIR(3));
    }

    int y2 = max_y / 2;
    int x2 = (max_x - wcswidth(red_str, wcslen(red_str))) / 2;

    attron(COLOR_PAIR(2));
    mvaddwstr(y2, x2, red_str);
    attroff(COLOR_PAIR(2));

    attron(COLOR_PAIR(3));
    mvaddwstr(y2 + 1, x2, blue_str);
    attroff(COLOR_PAIR(3));

    wchar_t msg4[] = L"Enter";
    int y3 = max_y / 4 * 3;
    int x3 = (max_x - wcswidth(msg4, wcslen(msg4))) / 2;
    mvaddwstr(y3, x3, msg4);
}

// 결과 전체
/*
SceneState result_function(int red_score, int blue_score, Result result) {

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color(); // 색상 기능 활성화

    // 색상 쌍 설정
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_BLUE, COLOR_BLACK);

    erase();

    draw_border();
    draw_result(red_score, blue_score, result);
        // if(scene_state == SCENE_ERROR || scene_state == SCENE_RESULT || scene_state == SCENE_EXIT )
           // return scene_state;
    refresh();

    while (1) {
        // draw_border();
        // draw_result(red_score, blue_score, result);
        // if(scene_state == SCENE_ERROR || scene_state == SCENE_RESULT || scene_state == SCENE_EXIT )
           // return scene_state;
        // refresh();

        int ch = getch();
        if (ch == 'q' || ch == 10 || ch == 13) break; // q 혹은 엔터 치면 꺼짐
    }

    endwin();
    scene_state = send_game_result(game_sock, token, result);
    return scene_state;
}*/

/*
int main() {

    int red_score = 3;
    int blue_score = 9;
    Result result = RESULT_WIN;
    // int red_score = 3;
    // int blue_score = 9;
    // Result result = RESULT_WIN;
    
    current_scene = result_function(red_score, blue_score, result);
    // result_function(red_score, blue_score, result);
    
    return 0;
}*/


void debug_log(const char* msg) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    attron(COLOR_PAIR(1));  // 노란색 (또는 원하는 색상)
    mvprintw(max_y - 2, 2, "DEBUG: %-*s", max_x - 4, msg); // 내용 덮어쓰기
    attroff(COLOR_PAIR(1));
    refresh();
}

SceneState result_function(int red_score, int blue_score, Result result, int winner_team) {
    initscr();
    //clear();
    //flushinp();
    cbreak();
    noecho();
    curs_set(0);
    start_color();
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_BLUE, COLOR_BLACK);

    // 창 크기 변경 핸들러 설정
    register_sigwinch_handler();  // signal() 대신 호출

    clear();
    draw_border();
    draw_result(red_score, blue_score, result, winner_team);
    refresh();
    nodelay(stdscr, TRUE);

    while (1) {
        if (resized) {
            resized = 0;

            endwin(); // ncurses 내부 상태 초기화
            refresh(); // 다시 초기화

            int new_y, new_x;
            getmaxyx(stdscr, new_y, new_x);
            resize_term(new_y, new_x);

            erase();
            draw_border();
            draw_result(red_score, blue_score, result, winner_team);
            refresh();
        }
        int ch = getch();
        if (ch != ERR) {
            char msg[64];
            snprintf(msg, sizeof(msg), "입력 감지: ch = %d", ch);  // ← ASCII 코드 출력
            debug_log(msg);
            if (ch == 'q' || ch == 10 || ch == 13) break;
        }

        
        usleep(100 * 1000); // 0.1초
    }

    endwin();
    //scene_state = send_game_result(game_sock, token, result);
    return SCENE_MAIN;
}