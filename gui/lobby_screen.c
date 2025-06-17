#include "client.h"
#include "lobby_screen.h"
#include "gui_utils.h"
#include "waiting_screen.h"
#include "codenames_screen.h"
#include <ncurses.h>
#include <locale.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
/*
static int current_selection = 0;
static char global_nickname[64] = "";
static GameInitInfo global_game_info;

int parse_game_init(const char *msg, GameInitInfo *info, const char *my_name) {
    if (strncmp(msg, "GAME_INIT|", 10) != 0) return -1;

    char *copy = strdup(msg + 10);
    if (!copy) return -1;

    char *token = strtok(copy, "|");
    int index = 0;

    while (token && index < 6) {
        strncpy(info->players[index].nickname, token, sizeof(info->players[index].nickname));
        token = strtok(NULL, "|");
        if (!token) break;
        info->players[index].role_num = atoi(token);
        token = strtok(NULL, "|");
        if (!token) break;
        info->players[index].team = atoi(token);
        token = strtok(NULL, "|");
        if (!token) break;
        info->players[index].is_leader = atoi(token);

        if (strcmp(info->players[index].nickname, my_name) == 0)
            info->myPlayerIndex = index;

        index++;
        token = strtok(NULL, "|");
    }

    free(copy);
    return (index == 6) ? 0 : -1;
}

void* wait_for_match_thread(void* arg) {
    TaskStatus* status = (TaskStatus*)arg;
    int sock = get_game_sock();
    if (sock <= 0) return NULL;

    set_waiting_message("매칭 대기 중...");
    char msg[256];
    snprintf(msg, sizeof(msg), "CMD|QUERY_WAIT|%s", token);
    send(sock, msg, strlen(msg), 0);

    char buf[1024] = {0};

    bool game_ready = false;

    while (!game_ready) {
        int bytes = recv(sock, buf, sizeof(buf) - 1, 0);
        if (bytes <= 0) {
            set_waiting_message("서버 응답 없음");
            update_task_status(status, 100, true, false);
            return NULL;
        }

        buf[bytes] = '\0';

        char* saveptr;
        char* line = strtok_r(buf, "\n", &saveptr);
        while (line) {
            if (strncmp(line, "WAIT_REPLY|", 11) == 0) {
                int count = atoi(line + 11);
                int prog = count * 100 / 6;
                if (prog >= 100) prog = 90;
                update_task_status(status, prog, false, false);
            } else if (strcmp(line, "QUEUE_FULL") == 0) {
                update_task_status(status, 90, false, false);
                set_cancel_enabled(false);
                send(sock, "READY_TO_GO\n", strlen("READY_TO_GO\n"), 0);
                set_waiting_message("모든 유저 준비 중...");
            } else if (strncmp(line, "GAME_INIT|", 10) == 0) {
                if (parse_game_init(line, &global_game_info, global_nickname) < 0) {
                    set_waiting_message("게임 정보 파싱 실패");
                    update_task_status(status, 100, true, false);
                    return NULL;
                }

                send(sock, "READY_TO_GO\n", strlen("READY_TO_GO\n"), 0);
                set_waiting_message("게임 준비 완료. 대기 중...");
            } else if (strncmp(line, "GAME_START|", 11) == 0) {
                char session_msg[128];
                snprintf(session_msg, sizeof(session_msg), "SESSION_READY|%s\n", token);
                send(sock, session_msg, strlen(session_msg), 0);

                set_waiting_message("🎮 게임 시작 준비 중...");
                update_task_status(status, 100, true, true);
                game_ready = true;
                break;
            }

            line = strtok_r(NULL, "\n", &saveptr);
        }
    }

    return NULL;
}



int parse_user_info(const char *response, UserInfo *info) {
    char *p = strchr(response, '|');
    if (!p) return -1;
    p++;

    char *nick = p;
    p = strchr(nick, '|');
    if (!p) return -1;
    *p = '\0';
    strncpy(info->nickname, nick, sizeof(info->nickname)-1);
    info->nickname[sizeof(info->nickname)-1] = '\0';
    p++;

    if (strncmp(p, "WINS|", 5) != 0) return -1;
    p += 5;
    info->wins = atoi(p);
    p = strchr(p, '|');
    if (!p) return -1;
    p++;

    if (strncmp(p, "LOSSES|", 7) != 0) return -1;
    p += 7;
    info->losses = atoi(p);

    return 0;
}

void draw_lobby_ui(const UserInfo* user) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;
    
    erase();
    draw_border();

    int art_start_y = 2;
    int art_start_x = (max_x - strlen(art[0])) / 2;
    for (int i = 0; i < art_lines; i++) {
        mvprintw(art_start_y + i, art_start_x, "%s", art[i]);
    }

    int info_y = art_start_y + art_lines + 2;

    char nickname_line[64];
    snprintf(nickname_line, sizeof(nickname_line), "닉네임: %s", user->nickname);
    mvprintw(info_y, (max_x - strlen(nickname_line)) / 2, "%s", nickname_line);

    char record_line[64];
    snprintf(record_line, sizeof(record_line), "전적: 승 %d / 패 %d", user->wins, user->losses);
    mvprintw(info_y + 2, (max_x - strlen(record_line)) / 2, "%s", record_line);

    double rate = (user->wins + user->losses) > 0
        ? (double)user->wins / (user->wins + user->losses) * 100.0 : 0.0;

    char rate_line[64];
    snprintf(rate_line, sizeof(rate_line), "승률: %.2f%%", rate);
    mvprintw(info_y + 4, (max_x - strlen(rate_line)) / 2, "%s", rate_line);

    const char* match_button = "[ 랜덤 매칭 시작 ]";
    if (current_selection == 0) attron(A_REVERSE);
    mvprintw(info_y + 7, (max_x - strlen(match_button)) / 2, "%s", match_button);
    if (current_selection == 0) attroff(A_REVERSE);

    refresh();
}

void sigwinch_handler_lobby(int signo) {
    endwin();
    refresh();
    clear();
}

SceneState lobby_screen(void) {
    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    signal(SIGWINCH, sigwinch_handler_lobby);

    char token_msg[TOKEN_LEN + 16];
    snprintf(token_msg, sizeof(token_msg), "TOKEN|%s", token);
    send(game_sock, token_msg, strlen(token_msg), 0);

    char buffer[256];
    int len = recv(game_sock, buffer, sizeof(buffer) - 1, 0);
    if (len <= 0) {
        perror("recv");
        close(game_sock);
        return SCENE_ERROR;
    }
    buffer[len] = '\0';
    printf("서버 응답: [%s]\n", buffer);

    if (strcmp(buffer, "INVALID_TOKEN") == 0) {
        close(game_sock);
        return SCENE_INVALID_TOKEN;
    }
    if (strcmp(buffer, "ERROR") == 0) {
        close(game_sock);
        return SCENE_ERROR;
    }

    UserInfo info;
    if (parse_user_info(buffer, &info) < 0) {
        fprintf(stderr, "서버 응답 파싱 실패\n");
        close(game_sock);
        return SCENE_ERROR;
    }

    draw_lobby_ui(&info);

    while (1) {
        int ch = getch();
        if (ch == 'q') break;

        switch (ch) {
            case KEY_UP:
            case KEY_DOWN:
                current_selection = 0;
                break;
            case 10:
                if (current_selection == 0) {
                    strncpy(global_nickname, info.nickname, sizeof(global_nickname) - 1);
                    global_nickname[sizeof(global_nickname) - 1] = '\0';
                    set_cancel_enabled(true);

                    TaskStatus status = {
                        .progress = 0,
                        .done = false,
                        .success = false,
                        .lock = PTHREAD_MUTEX_INITIALIZER
                    };

                    WaitingResult result = waiting(&status, wait_for_match_thread);
                    if (result == WAIT_SUCCESS) {
                        endwin();
                        return codenames_screen(global_game_info);
                    } else if(result == WAIT_CANCELED) {
                        char cancel_msg[128];
                        snprintf(cancel_msg, sizeof(cancel_msg), "MATCHING_CANCEL|%s", token);
                        send(get_game_sock(), cancel_msg, strlen(cancel_msg), 0);

                        char response[128] = {0};
                        int res_len = recv(get_game_sock(), response, sizeof(response) - 1, 0);
                        if (res_len > 0) {
                            response[res_len] = '\0';
                            if (strcmp(response, "CANCEL_OK") == 0) {
                                set_waiting_message("매칭이 취소되었습니다.");
                                sleep(1);  // 사용자에게 메시지를 짧게 보여주기
                            } else {
                                // 실패 처리
                                set_waiting_message("매칭 취소 실패");
                                sleep(1);
                            }
                        } else {
                            set_waiting_message("서버 응답 없음");
                            sleep(1);  // 사용자에게 메시지를 짧게 보여주기
                        }
                    }
                }
                break;
        }

        draw_lobby_ui(&info);
    }

    endwin();
    return SCENE_MAIN;
}
*/

#include "client.h"
#include "lobby_screen.h"
#include "gui_utils.h"
// #include "connect_to_server.h"
#include "waiting_screen.h"
#include "codenames_screen.h"
#include <ncurses.h>
#include <locale.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define LOBBY_MAX_INPUT 32

typedef enum {
    LOBBY_STATE_NICKNAME_INPUT,         // 닉네임 기본 입력 상태
    LOBBY_STATE_NICKNAME_EDIT_BUTTON,   // [닉네임 수정] 버튼 선택됨
    LOBBY_STATE_MATCH_BUTTON             // [랜덤 매칭 시작] 버튼 선택됨
} LobbyUIState;

typedef enum {
    LOBBY_AVAILABLE = 0,     // 사용 가능
    LOBBY_DUPLICATED = -1,   // 중복
    LOBBY_ERROR = -2,         // 에러
    LOBBY_NOT_CHECKED = -3   // 아직 체크하지 않음
} LobbyCheckNickname;

typedef enum {
    NICKNAME_EDIT_NONE = -1,
    NICKNAME_EDIT_SUCCESS = 0,    // 회원가입 성공
    NICKNAME_EDIT_ERROR = 1,      // 에러 (네트워크, 서버 등)
    NICKNAME_INVALID_TOCKEN = 2
} EditNickNameResult;

static LobbyUIState current_state = LOBBY_STATE_MATCH_BUTTON;
static char nickname_buf[LOBBY_MAX_INPUT] = {0};
static int nicknameEditPending = 0; // 닉네임 수정중 : 1, 닉네임 수정 완료 : 0
static LobbyCheckNickname nickname_available = LOBBY_NOT_CHECKED;
static EditNickNameResult nickname_edit_result = NICKNAME_EDIT_NONE;
static int is_nickname_successfully_modified = 0;
static int nickname_y = 0;
static int nickname_x = 0;
static int nickname_check = 0;
static int init = 0;
static int matching_state = 0;

static int current_selection = 0;
static char global_nickname[64] = "";
static GameInitInfo global_game_info;


int parse_game_init(const char *msg, GameInitInfo *info, const char *my_name) {
    if (strncmp(msg, "GAME_INIT|", 10) != 0) return -1;

    char *copy = strdup(msg + 10);
    if (!copy) return -1;

    char *token = strtok(copy, "|");
    int index = 0;

    while (token && index < 6) {
        strncpy(info->players[index].nickname, token, sizeof(info->players[index].nickname));
        token = strtok(NULL, "|");
        if (!token) break;
        info->players[index].role_num = atoi(token);
        token = strtok(NULL, "|");
        if (!token) break;
        info->players[index].team = atoi(token);
        token = strtok(NULL, "|");
        if (!token) break;
        info->players[index].is_leader = atoi(token);

        if (strcmp(info->players[index].nickname, my_name) == 0)
            info->myPlayerIndex = index;

        index++;
        token = strtok(NULL, "|");
    }

    free(copy);
    return (index == 6) ? 0 : -1;
}

void* wait_for_match_thread(void* arg) {
    TaskStatus* status = (TaskStatus*)arg;
    int sock = get_game_sock();
    if (sock <= 0) return NULL;

    set_waiting_message("매칭 대기 중...");
    char msg[256];
    snprintf(msg, sizeof(msg), "CMD|QUERY_WAIT|%s", token);
    send(sock, msg, strlen(msg), 0);

    char buf[1024] = {0};

    bool game_ready = false;

    while (!game_ready) {
        int bytes = recv(sock, buf, sizeof(buf) - 1, 0);
        if (bytes <= 0) {
            set_waiting_message("서버 응답 없음");
            update_task_status(status, 100, true, false);
            return NULL;
        }

        buf[bytes] = '\0';

        char* saveptr;
        char* line = strtok_r(buf, "\n", &saveptr);
        while (line) {
            if (strncmp(line, "WAIT_REPLY|", 11) == 0) {
                int count = atoi(line + 11);
                int prog = count * 100 / 6;
                if (prog >= 100) prog = 90;
                update_task_status(status, prog, false, false);
            } else if (strcmp(line, "QUEUE_FULL") == 0) {
                update_task_status(status, 90, false, false);
                set_cancel_enabled(false);
                send(sock, "READY_TO_GO\n", strlen("READY_TO_GO\n"), 0);
                set_waiting_message("모든 유저 준비 중...");
            } else if (strncmp(line, "GAME_INIT|", 10) == 0) {
                if (parse_game_init(line, &global_game_info, global_nickname) < 0) {
                    set_waiting_message("게임 정보 파싱 실패");
                    update_task_status(status, 100, true, false);
                    return NULL;
                }

                send(sock, "READY_TO_GO\n", strlen("READY_TO_GO\n"), 0);
                set_waiting_message("게임 준비 완료. 대기 중...");
            } else if (strncmp(line, "GAME_START|", 11) == 0) {
                char session_msg[128];
                snprintf(session_msg, sizeof(session_msg), "SESSION_READY|%s\n", token);
                send(sock, session_msg, strlen(session_msg), 0);

                set_waiting_message("🎮 게임 시작 준비 중...");
                update_task_status(status, 100, true, true);
                game_ready = true;
                break;
            }

            line = strtok_r(NULL, "\n", &saveptr);
        }
    }

    return NULL;
}

// echo_mode 갱신
static void update_echo_mode() {
    if (current_state == LOBBY_STATE_NICKNAME_INPUT) {
        echo();
        curs_set(1);
    } else {
        noecho();
        curs_set(0);
    }
}

// 서버로부터 받은 정보 파싱 (NICKNAME|닉네임|WINS|10|LOSSES|5)
int parse_user_info(const char *response, UserInfo *info) {
    char *p = strchr(response, '|');
    if (!p) return -1;
    p++;

    // 닉네임
    char *nick = p;
    p = strchr(nick, '|');
    if (!p) return -1;
    *p = '\0';
    strncpy(info->nickname, nick, sizeof(info->nickname)-1);
    info->nickname[sizeof(info->nickname)-1] = '\0';
    p++;

    strncpy(nickname_buf, info->nickname, sizeof(nickname_buf) - 1);
    nickname_buf[sizeof(nickname_buf) - 1] = '\0';

    // WINS
    if (strncmp(p, "WINS|", 5) != 0) return -1;
    p += 5;
    info->wins = atoi(p);
    p = strchr(p, '|');
    if (!p) return -1;
    p++;

    // LOSSES
    if (strncmp(p, "LOSSES|", 7) != 0) return -1;
    p += 7;
    info->losses = atoi(p);

    return 0;
}

// 닉네임 중복 확인
LobbyCheckNickname lobby_check_nickname_availability(const char *nickname) {
    if (game_sock < 0) {  // 소켓이 열려있지 않으면 에러
        perror("Invalid socket");
        return LOBBY_ERROR;
    }

    char request[256];
    snprintf(request, sizeof(request), "CHECK_NICKNAME|%s", nickname);

    // 요청 보내기
    ssize_t sent = send(game_sock, request, strlen(request), 0);
    if (sent <= 0) {
        perror("send");
        return LOBBY_ERROR;
    }

    // 응답 받기
    char response[256] = {0};
    ssize_t recv_len = recv(game_sock, response, sizeof(response)-1, 0);
    if (recv_len <= 0) {
        perror("recv");
        return LOBBY_ERROR;
    }
    response[recv_len] = '\0';

    if (strncmp(response, "NICKNAME_AVAILABLE", 18) == 0) {
        return LOBBY_AVAILABLE;
    } else if (strncmp(response, "NICKNAME_TAKEN", 14) == 0) {
        return LOBBY_DUPLICATED;
    } else {
        return LOBBY_ERROR;
    }
}

// 닉네임 수정 요청
EditNickNameResult send_nickname_edit_request(const char *nickname) {
    // 요청 패킷 예시: "EDIT_NICK|토큰|새닉네임"
    char request[256];
    snprintf(request, sizeof(request), "EDIT_NICK|%s|%s", token, nickname);

    // SSL_write로 요청 전송
    if (SSL_write(ssl, request, strlen(request)) <= 0) {
        perror("SSL_write");
        return NICKNAME_EDIT_ERROR;
    }

    // SSL_read로 응답 수신
    char response[256] = {0};
    int recv_len = SSL_read(ssl, response, sizeof(response)-1);
    if (recv_len <= 0) {
        perror("SSL_read");
        return NICKNAME_EDIT_ERROR;
    }
    response[recv_len] = '\0';

    // 응답 파싱
    if (strstr(response, "NICKNAME_EDIT_OK")) {
        return NICKNAME_EDIT_SUCCESS;
    } else if (strcmp(response, "INVALID_TOKEN") == 0) {
        return NICKNAME_INVALID_TOCKEN;
    } else {
        return NICKNAME_EDIT_ERROR;
    }
}

void draw_lobby_ui(const UserInfo* user) {

    update_echo_mode();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    erase();
    start_color(); // 색상 기능 활성화
    init_pair(1, COLOR_RED, COLOR_BLACK); // 색상 쌍 설정
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    draw_border(); // 테두리 출력

    // 로고 출력
    int logo_start_x = (max_x - strlen(art[0])) / 2;
    int logo_start_y = (max_y - art_lines - 8) / 4;
    for (int i = 0; i < art_lines; i++) {
        mvprintw(logo_start_y + i, logo_start_x, "%s", art[i]);
    }

    // 닉네임
    int input_start_x = logo_start_x+18;
    int input_start_y = logo_start_y + art_lines + 4; 

    nickname_y = input_start_y;
    nickname_x = input_start_x + strlen("닉네임 : ");


    char nickname_line[64];
    if (current_state == LOBBY_STATE_MATCH_BUTTON && is_nickname_successfully_modified == 1) {
        mvprintw(input_start_y, input_start_x, "닉네임 : ");
        mvprintw(input_start_y, input_start_x + 9, "%s", nickname_buf);
    } else if (nicknameEditPending == 1 && LOBBY_STATE_MATCH_BUTTON) {
        mvprintw(input_start_y, input_start_x, "닉네임 : ");
        mvprintw(input_start_y, input_start_x + 9, "%s", nickname_buf);
    } 
    else if((current_state == LOBBY_STATE_MATCH_BUTTON && init == 0)) {
        snprintf(nickname_line, sizeof(nickname_line), "닉네임 : %s", user->nickname);
        mvprintw(input_start_y, input_start_x, "%s", nickname_line);
    } else {
        mvprintw(input_start_y, input_start_x, "닉네임 : ");
        mvprintw(input_start_y, input_start_x + 9, "%-*s", LOBBY_MAX_INPUT, nickname_buf);
    }

    if(current_state == LOBBY_STATE_MATCH_BUTTON || current_state == LOBBY_STATE_NICKNAME_INPUT) {
        mvprintw(input_start_y, input_start_x + 39, "%s", "[닉네임 수정]");
    } else if (current_state == LOBBY_STATE_NICKNAME_EDIT_BUTTON){
        if (current_state == LOBBY_STATE_NICKNAME_EDIT_BUTTON) attron(A_REVERSE);
        mvprintw(input_start_y, input_start_x + 39, "%s", "[닉네임 수정]");
        if (current_state == LOBBY_STATE_NICKNAME_EDIT_BUTTON) attroff(A_REVERSE);
    }

    if (nickname_available == LOBBY_DUPLICATED) {
        attron(COLOR_PAIR(1)); 
        mvprintw(input_start_y + 1, input_start_x + 8, "이미 사용 중인 닉네임입니다.");
        attroff(COLOR_PAIR(1));
    } else if (nickname_edit_result == NICKNAME_EDIT_SUCCESS) {
        attron(COLOR_PAIR(2)); 
        mvprintw(input_start_y + 1, input_start_x + 8, "닉네임이 성공적으로 수정되었습니다.");
        nicknameEditPending = 0;
        nickname_check = 0;
        init = 0;
        attroff(COLOR_PAIR(2));
    } else if (init == 1) {
        attron(COLOR_PAIR(2)); 
        mvprintw(input_start_y + 1, input_start_x + 8, "엔터 혹은 [닉네임 수정] 버튼을 선택하여야 닉네임이 수정됩니다.");
        attroff(COLOR_PAIR(2));
    } 

    // 역대 전적
    char record_line[64];
    snprintf(record_line, sizeof(record_line), "전적: 승 %d / 패 %d", user->wins, user->losses);
    mvprintw(input_start_y + 5, (max_x - strlen(record_line)) / 2, "%s", record_line);

    double rate = (user->wins + user->losses) > 0
                ? (double)user->wins / (user->wins + user->losses) * 100.0
                : 0.0;

    char rate_line[64];
    snprintf(rate_line, sizeof(rate_line), "승률: %.2f%%", rate);
    mvprintw(input_start_y + 9, (max_x - strlen(rate_line)) / 2 - 2, "%s", rate_line);

    // 매칭 시작
    const char* match_button = "[ 랜덤 매칭 시작 ]";
    if (current_state ==  LOBBY_STATE_MATCH_BUTTON) attron(A_REVERSE);
    mvprintw(input_start_y + 13, (max_x - strlen(match_button)) / 2, "%s", match_button);
    if (current_state == LOBBY_STATE_MATCH_BUTTON) attroff(A_REVERSE);

    refresh();
    if(current_state == LOBBY_STATE_NICKNAME_INPUT) {
        move(nickname_y, nickname_x + strlen(nickname_buf) - 3);
    }
}

void sigwinch_handler_lobby(int signo) {
    endwin();
    refresh();
    clear();
}

SceneState lobby_screen(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    erase();
    signal(SIGWINCH, sigwinch_handler_lobby);
    nickname_available = LOBBY_NOT_CHECKED;
    init = 0;

    // 토큰을 서버에 전송 (예: "TOKEN|토큰값")
    char token_msg[TOKEN_LEN + 16];
    snprintf(token_msg, sizeof(token_msg), "TOKEN|%s", token);
    send(game_sock, token_msg, strlen(token_msg), 0);

    // 서버로부터 닉네임, 승/패 횟수 받기
    char buffer[256];
    int len = recv(game_sock, buffer, sizeof(buffer) - 1, 0);
    if (len <= 0) {
        perror("recv");
        close(game_sock);
        return SCENE_ERROR;
    }
    buffer[len] = '\0';
    // printf("서버 응답: [%s]\n", buffer);

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

    // 서버 응답 파싱 (NICKNAME|닉네임|WINS|10|LOSSES|5)
    UserInfo info;
    if (parse_user_info(buffer, &info) < 0) {
        fprintf(stderr, "서버 응답 파싱 실패\n");
        close(game_sock);
        return SCENE_ERROR;
    }

    draw_lobby_ui(&info);

    while (1) {
        int ch = getch();
        if (ch == 'q') break;

        switch (ch) {
            case KEY_UP:
                if (current_state == LOBBY_STATE_MATCH_BUTTON) {
                    current_state = LOBBY_STATE_NICKNAME_INPUT;
                    init = 1;
                    break;
                }
            case KEY_DOWN:
                if (current_state == LOBBY_STATE_NICKNAME_INPUT) current_state = LOBBY_STATE_MATCH_BUTTON;
                else if (current_state == LOBBY_STATE_NICKNAME_EDIT_BUTTON) current_state = LOBBY_STATE_MATCH_BUTTON;
                matching_state = 1;
                break;
            case KEY_LEFT:
                if (current_state == LOBBY_STATE_NICKNAME_EDIT_BUTTON) current_state = LOBBY_STATE_NICKNAME_INPUT;
                break;
            case KEY_RIGHT:
                if (current_state == LOBBY_STATE_NICKNAME_INPUT) current_state = LOBBY_STATE_NICKNAME_EDIT_BUTTON;
                break;
            case 10: // Enter
                if (current_state == LOBBY_STATE_MATCH_BUTTON){
                    strncpy(global_nickname, info.nickname, sizeof(global_nickname) - 1);
                    global_nickname[sizeof(global_nickname) - 1] = '\0';
                    set_cancel_enabled(true);

                    TaskStatus status = {
                        .progress = 0,
                        .done = false,
                        .success = false,
                        .lock = PTHREAD_MUTEX_INITIALIZER
                    };

                    WaitingResult result = waiting(&status, wait_for_match_thread);
                    if (result == WAIT_SUCCESS) {
                        endwin();
                        return codenames_screen(global_game_info);
                    } else if(result == WAIT_CANCELED) {
                        char cancel_msg[128];
                        snprintf(cancel_msg, sizeof(cancel_msg), "MATCHING_CANCEL|%s", token);
                        send(get_game_sock(), cancel_msg, strlen(cancel_msg), 0);

                        char response[128] = {0};
                        int res_len = recv(get_game_sock(), response, sizeof(response) - 1, 0);
                        if (res_len > 0) {
                            response[res_len] = '\0';
                            if (strcmp(response, "CANCEL_OK") == 0) {
                                set_waiting_message("매칭이 취소되었습니다.");
                                sleep(1);  // 사용자에게 메시지를 짧게 보여주기
                            } else {
                                // 실패 처리
                                set_waiting_message("매칭 취소 실패");
                                sleep(1);
                            }
                        } else {
                            set_waiting_message("서버 응답 없음");
                            sleep(1);  // 사용자에게 메시지를 짧게 보여주기
                        }
                    }
                    break;
                } else if (current_state == LOBBY_STATE_NICKNAME_INPUT || current_state == LOBBY_STATE_NICKNAME_EDIT_BUTTON) {
                    matching_state = 1;
                    nickname_available = lobby_check_nickname_availability(nickname_buf);
                    nickname_check = 1;
                    if(nickname_available == LOBBY_ERROR) {
                        return SCENE_ERROR;
                    } else if(nickname_available == LOBBY_AVAILABLE) {
                        nickname_edit_result = send_nickname_edit_request(nickname_buf);
                        if(nickname_edit_result == NICKNAME_INVALID_TOCKEN)
                            return SCENE_INVALID_TOKEN;
                        else if (nickname_edit_result == NICKNAME_EDIT_ERROR)
                            return SCENE_ERROR;
                        else {
                            nicknameEditPending = 0;
                            is_nickname_successfully_modified = 1;
                        }
                    }
                    break;
                } 
            case 127: // 백스페이스 눌렀을 때
            case KEY_BACKSPACE:
                if (current_state == LOBBY_STATE_NICKNAME_INPUT && strlen(nickname_buf) > 0) {
                    int nick_len = strlen(nickname_buf);
                    nickname_buf[nick_len - 1] = '\0';
                    mvaddch(nickname_y, nickname_x + nick_len - 1, ' ');
                }
                draw_lobby_ui(&info);
                break;
            default: // 문자 입력
                init = 1;
                if (ch == 27) { // 화살표 처리 : ESC 키 -> 다음 두 문자 읽어서 버림 (예: [C, [D, [A 등 처리)
                    getch();
                    getch();
                    break;
                }
                else if (ch >= 32 && ch <= 126) {
                    nicknameEditPending = 1;
                    is_nickname_successfully_modified = 0;
                    if (current_state == LOBBY_STATE_NICKNAME_INPUT && strlen(nickname_buf) < MAX_INPUT - 1) {
                        strncat(nickname_buf, (char*)&ch, 1);
                        mvaddch(nickname_y, nickname_x + strlen(nickname_buf) - 1, ch);
                    }
                }
                break;
        }
        draw_lobby_ui(&info);
    }

    endwin();
    return SCENE_MAIN;
}