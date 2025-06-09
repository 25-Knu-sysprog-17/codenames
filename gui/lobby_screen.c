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
