#include "session.h"
#include "user_info.h"
#include "user_report.h"
#include "game_result.h"
#include "room_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#define RESPONSE_BUFFER 1024
#define MAX_SESSIONS 10

// 총 게임 세션(진행 중 게임들)
static GameSession* ongoing_sessions[MAX_SESSIONS] = {0};
static pthread_mutex_t ongoing_sessions_mutex = PTHREAD_MUTEX_INITIALIZER;

// ⏳ 대기 큐 관련
static PlayerInfo waiting_queue[MAX_ROOM_PLAYERS];
static int waiting_count = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

// 🔁 READY 동기화 변수
static int ready_phase1 = 0;
static int ready_phase2 = 0;
static pthread_mutex_t ready_mutex = PTHREAD_MUTEX_INITIALIZER;

// 🎯 매칭 요청 처리
WaitResult handle_waiting_command(int client_sock, const char* token) {
    pthread_mutex_lock(&queue_mutex);

    // 클라이언트를 큐에 추가
    strncpy(waiting_queue[waiting_count].token, token, TOKEN_LEN - 1);
    waiting_queue[waiting_count].token[TOKEN_LEN - 1] = '\0';
    waiting_queue[waiting_count].sock = client_sock;
    waiting_count++;

    int count_snapshot = waiting_count;

    if (waiting_count == MAX_ROOM_PLAYERS) {
        // 방 인원이 다 차면 바로 방을 생성
        MatchRoomArgs* args = malloc(sizeof(MatchRoomArgs));
        memcpy(args->players, waiting_queue, sizeof(waiting_queue));

        pthread_t tid;
        pthread_create(&tid, NULL, run_match_room, args);
        pthread_detach(tid);

        // 모든 클라이언트에게 QUEUE_FULL 전송
        const char* msg = "QUEUE_FULL\n";
        for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
            send(waiting_queue[i].sock, msg, strlen(msg), 0);
        }

        waiting_count = 0;
        pthread_mutex_unlock(&queue_mutex);
        return WAIT_GAME_STARTED;
    } else {
        // 아직 인원이 부족할 경우, 모든 대기 중 클라이언트에게 WAIT_REPLY 전송
        char reply[64];
        snprintf(reply, sizeof(reply), "WAIT_REPLY|%d\n", count_snapshot);
        for (int i = 0; i < waiting_count; ++i) {
            send(waiting_queue[i].sock, reply, strlen(reply), 0);
            printf("↪️ WAIT_REPLY|%d sent to token=%s (sock=%d)\n",
                count_snapshot,
                waiting_queue[i].token,
                waiting_queue[i].sock);
        }

        pthread_mutex_unlock(&queue_mutex);
        return WAIT_CONTINUE;
    }
}

// ❌ 매칭 취소 요청 처리
WaitResult cancel_waiting(int client_sock, const char* token) {
    pthread_mutex_lock(&queue_mutex);

    int index = -1;
    for (int i = 0; i < waiting_count; ++i) {
        if (waiting_queue[i].sock == client_sock &&
            strncmp(waiting_queue[i].token, token, TOKEN_LEN) == 0) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        pthread_mutex_unlock(&queue_mutex);
        return WAIT_ERROR;
    }

    for (int i = index; i < waiting_count - 1; ++i) {
        waiting_queue[i] = waiting_queue[i + 1];
    }
    waiting_count--;

    if (waiting_count > 0) {
        char reply[64];
        snprintf(reply, sizeof(reply), "WAIT_REPLY|%d\n", waiting_count);
        for (int i = 0; i < waiting_count; ++i) {
            send(waiting_queue[i].sock, reply, strlen(reply), 0);
        }
    }

    pthread_mutex_unlock(&queue_mutex);
    return WAIT_CANCELLED;
}

// 💬 클라이언트의 TCP 요청 처리하는 서브 루프
void* client_response_loop(int client_sock, const char* token) {
    char buffer[RESPONSE_BUFFER];
    GameSession* session = NULL;
    int my_index = -1;
    bool loopEnd = false;

    printf("%s- Game Session Started\n", token);
    while (!loopEnd) {
        int len = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) {
            cancel_waiting(client_sock, token);
            break;
        }

        buffer[len] = '\0';

        char* saveptr;
        char* line = strtok_r(buffer, "\n", &saveptr);
        while (line) {
            printf("%s- Client request: %s\n", token, line);

            if (strncmp(line, "SESSION_READY|", 14) == 0) {
                const char* received_token = line + 14;
                session = find_session_by_token(received_token);

                if (session) {
                    for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
                        if (strcmp(session->init_info.players[i].info.token, received_token) == 0) {
                            my_index = i;
                            break;
                        }
                    }
                    send(client_sock, "SESSION_ACK\n", strlen("SESSION_ACK\n"), 0);
                } else {
                    send(client_sock, "SESSION_NOT_FOUND\n", strlen("SESSION_NOT_FOUND\n"), 0);
                }
            } else if (strncmp(line, "MATCHING_CANCEL|", 16) == 0) {
                const char* received_token = line + 16;
                if (strncmp(received_token, token, TOKEN_LEN) == 0) {
                    WaitResult res = cancel_waiting(client_sock, token);
                    send(client_sock, "CANCEL_OK\n", strlen("CANCEL_OK\n"), 0);
                    if (res == WAIT_CANCELLED || WAIT_ERROR)
                        loopEnd = true;
                    break;
                } else {
                    send(client_sock, "CANCEL_OK\n", strlen("CANCEL_OK\n"), 0);
                    break;
                }
            } else if (strcmp(line, "READY_TO_GO") == 0) {
                pthread_mutex_lock(&ready_mutex);
                if (ready_phase1 < MAX_ROOM_PLAYERS)
                    ready_phase1++;
                else
                    ready_phase2++;
                pthread_mutex_unlock(&ready_mutex);
            } else if (strncmp(line, "CHAT|", 5) == 0 && session && my_index != -1) {
                GameEvent ev = {.type = EVENT_CHAT, .player_index = my_index};
                strncpy(ev.data, line, sizeof(ev.data) - 1);
                if (!enqueue_game_event(session, ev)) {
                    send(client_sock, "EVENT_QUEUE_FULL\n", strlen("EVENT_QUEUE_FULL\n"), 0);
                }
            } else if (strncmp(line, "HINT|", 5) == 0 && session && my_index != -1) {
                GameEvent ev = {.type = EVENT_HINT, .player_index = my_index};
                strncpy(ev.data, line, sizeof(ev.data) - 1);
                if (!enqueue_game_event(session, ev)) {
                    send(client_sock, "EVENT_QUEUE_FULL\n", strlen("EVENT_QUEUE_FULL\n"), 0);
                }
            } else if (strncmp(line, "ANSWER|", 7) == 0 && session && my_index != -1) {
                GameEvent ev = {.type = EVENT_ANSWER, .player_index = my_index};
                strncpy(ev.data, line, sizeof(ev.data) - 1);
                if (!enqueue_game_event(session, ev)) {
                    send(client_sock, "EVENT_QUEUE_FULL\n", strlen("EVENT_QUEUE_FULL\n"), 0);
                }
            } else if (strcmp(line, "GET_ROLE") == 0 && session && my_index != -1) {
                char role_msg[64];
                snprintf(role_msg, sizeof(role_msg), "ROLE_INFO|%d\n", session->init_info.players[my_index].role_num);
                send(client_sock, role_msg, strlen(role_msg), 0);
            } else if (strcmp(line, "GET_ALL_CARDS") == 0 && session) {
                char cards_msg[1024] = "ALL_CARDS";
                for (int i = 0; i < MAX_CARDS; ++i) {
                    char entry[48];
                    snprintf(entry, sizeof(entry), "|%s|%d|%d",
                             session->cards[i].name,
                             session->cards[i].type,
                             session->cards[i].isUsed);

                    if (strlen(cards_msg) + strlen(entry) >= sizeof(cards_msg) - 1) break;
                    strncat(cards_msg, entry, sizeof(cards_msg) - strlen(cards_msg) - 1);
                }
                strncat(cards_msg, "\n", sizeof(cards_msg) - strlen(cards_msg) - 1);
                send(client_sock, cards_msg, strlen(cards_msg), 0);
                printf(">>> To [%d]: %s", client_sock, cards_msg);
            } else if (strncmp(line, "REPORT|", 7) == 0) {
                char* token_ptr = line + 7;
                char* nickname = strchr(token_ptr, '|');
                if (!nickname) {
                    send(client_sock, "REPORT_ERROR|INVALID_REQUEST\n", 30, 0);
                    continue;
                }
                *nickname++ = '\0';

                char user_id[64], target_id[64];
                if (get_user_id_by_token(token_ptr, user_id, sizeof(user_id)) != 0) {
                    send(client_sock, "REPORT_ERROR|INVALID_TOKEN\n", 28, 0);
                    continue;
                }

                if (get_user_id_by_nickname(nickname, target_id) != 0) {
                    send(client_sock, "REPORT_ERROR|USER_NOT_FOUND\n", 29, 0);
                    continue;
                }

                int report_count = report_user(target_id);
                if (report_count < 0) {
                    send(client_sock, "REPORT_ERROR|DB_ERROR\n", 23, 0);
                    continue;
                }

                char response[128];
                if (report_count >= 3) {
                    snprintf(response, sizeof(response), "REPORT_OK|%d|SUSPENDED\n", report_count);
                } else {
                    snprintf(response, sizeof(response), "REPORT_OK|%d\n", report_count);
                }
                send(client_sock, response, strlen(response), 0);
            } else if (strcmp(line, "GAME_QUIT") == 0) {
                printf("%s- Client exiting after GAME_OVER\n", token);
                loopEnd = true;
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }
    }
    return NULL;
}

// 🧩 게임 시작 처리
void* run_match_room(void* arg) {
    MatchRoomArgs* args = (MatchRoomArgs*)arg;

    while (1) {
        pthread_mutex_lock(&ready_mutex);
        if (ready_phase1 == MAX_ROOM_PLAYERS) {
            pthread_mutex_unlock(&ready_mutex);
            break;
        }
        pthread_mutex_unlock(&ready_mutex);
        usleep(10000);
    }

    GameInitInfo game_info;
    for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
        char nickname[64] = "UNKNOWN";
        get_nickname_by_token(args->players[i].token, nickname, sizeof(nickname));
        strncpy(game_info.players[i].nickname, nickname, sizeof(game_info.players[i].nickname) - 1);
        game_info.players[i].role_num = i;
        game_info.players[i].team = (i < 3) ? 0 : 1;
        game_info.players[i].is_leader = (i == 0 || i == 3);
        memcpy(&game_info.players[i].info, &args->players[i], sizeof(PlayerInfo));
    }

    char init_msg[1024] = "GAME_INIT";
    for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
        char entry[128];
        snprintf(entry, sizeof(entry), "|%s|%d|%d|%d",
                 game_info.players[i].nickname,
                 game_info.players[i].role_num,
                 game_info.players[i].team,
                 game_info.players[i].is_leader);
        strncat(init_msg, entry, sizeof(init_msg) - strlen(init_msg) - 1);
    }

    for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
        char with_nl[1080];
        snprintf(with_nl, sizeof(with_nl), "%s\n", init_msg);
        send(args->players[i].sock, with_nl, strlen(with_nl), 0);
    }

    while (1) {
        pthread_mutex_lock(&ready_mutex);
        if (ready_phase2 == MAX_ROOM_PLAYERS) {
            pthread_mutex_unlock(&ready_mutex);
            break;
        }
        pthread_mutex_unlock(&ready_mutex);
        usleep(10000);
    }

    GameSession* session = malloc(sizeof(GameSession));
    memset(session, 0, sizeof(GameSession));
    pthread_mutex_init(&session->game_lock, NULL);
    pthread_mutex_init(&session->event_lock, NULL);

    for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
        session->init_info.players[i] = game_info.players[i];
    }
    session->init_info = game_info;

    if (!assign_cards("words.txt", session->cards)) {
        fprintf(stderr, "카드 초기화 실패: 게임 세션 종료\n");
        const char* fail_msg = "GAME_FAILED|카드 초기화 오류\n";
        for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
            send(args->players[i].sock, fail_msg, strlen(fail_msg), 0);
        }
        unregister_session(session);
        free(session);
        free(args);
        return NULL;
    }

    session->turn_team = 0;
    session->phase = 0;
    session->redScore = 0;
    session->blueScore = 0;
    session->game_over = false;
    session->event_front = 0;
    session->event_rear = 0;
    session->hint_word[0] = '\0';
    session->hint_count = 0;
    session->remaining_tries = 0;

    int session_id = register_session(session);
    if (session_id == -1) {
        fprintf(stderr, "세션 등록 실패\n");
        free(session);
        free(args);
        return NULL;
    }

    char start_msg[64];
    snprintf(start_msg, sizeof(start_msg), "GAME_START|%d\n", session_id);
    for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
        send(args->players[i].sock, start_msg, strlen(start_msg), 0);
    }

    pthread_t game_thread;
    pthread_create(&game_thread, NULL, run_game_session, session);
    pthread_detach(game_thread);

    pthread_mutex_lock(&ready_mutex);
    ready_phase1 = 0;
    ready_phase2 = 0;
    pthread_mutex_unlock(&ready_mutex);

    free(args);
    return NULL;
}


/*
    ///////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////
    
    여기서 부터 게임 세션 코드임
*/

void assign_card_types(GameCard cards[MAX_CARDS]) {
    int types[MAX_CARDS];
    int index = 0;

    for (int i = 0; i < 9; ++i) types[index++] = RED_CARD;
    for (int i = 0; i < 8; ++i) types[index++] = BLUE_CARD;
    for (int i = 0; i < 7; ++i) types[index++] = NEUTRAL_CARD;
    types[index++] = ASSASSIN_CARD;

    // Fisher-Yates shuffle
    srand(time(NULL));
    for (int i = MAX_CARDS - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int tmp = types[i];
        types[i] = types[j];
        types[j] = tmp;
    }

    for (int i = 0; i < MAX_CARDS; ++i) {
        strncpy(cards[i].name, "", sizeof(cards[i].name));  // 이름은 이후 단어 리스트로 대입됨
        cards[i].type = types[i];
        cards[i].isUsed = 0;
    }
}

// 🕹️ 게임 세션 루프 (추후 힌트, 정답 처리 등 구현 예정)
void* run_game_session(void* arg) {
    GameSession* session = (GameSession*) arg;

    char start_msg[48];
    snprintf(start_msg, sizeof(start_msg), "CHAT|%d|%d|%s|%s",
                                    SYSTEM_TEAM, 0, SYSTEM_NAME, "게임 시작!");
    broadcast_to_all(session, start_msg);

    char init_msg[48];
    snprintf(init_msg, sizeof(init_msg), "TURN_UPDATE|%d|%d|%d|%d",
             session->turn_team,
             session->phase,
             session->redScore,
             session->blueScore);
    broadcast_to_all(session, init_msg);

    while (!session->game_over) {
        pthread_mutex_lock(&session->event_lock);

        if (session->event_front == session->event_rear) {
            pthread_mutex_unlock(&session->event_lock);
            usleep(50000);
            continue;
        }

        GameEvent ev = session->event_queue[session->event_front];
        session->event_front = (session->event_front + 1) % MAX_EVENTS;
        pthread_mutex_unlock(&session->event_lock);

        pthread_mutex_lock(&session->game_lock);

        int player_team = session->init_info.players[ev.player_index].team;
        bool is_leader = session->init_info.players[ev.player_index].is_leader;

        if (ev.type == EVENT_HINT && session->phase == 0 && is_leader && player_team == session->turn_team) {
            // HINT|단어|숫자
            char* word = strtok(ev.data + 5, "|");
            char* number = strtok(NULL, "|");
            if (word && number) {
                strncpy(session->hint_word, word, sizeof(session->hint_word) - 1);
                session->hint_word[sizeof(session->hint_word) - 1] = '\0';
                session->hint_count = atoi(number);
                session->remaining_tries = session->hint_count;
                session->phase = 1;

                // 힌트 및 턴 정보 전송
                char hint_msg[128];
                snprintf(hint_msg, sizeof(hint_msg), "HINT|%d|%s|%d",
                        session->turn_team, word, session->hint_count);
                broadcast_to_all(session, hint_msg);

                char turn_msg[64];
                snprintf(turn_msg, sizeof(turn_msg), "TURN_UPDATE|%d|%d|%d|%d",
                        session->turn_team, session->phase, session->redScore, session->blueScore);
                broadcast_to_all(session, turn_msg);
            }
        } else if (ev.type == EVENT_ANSWER && session->phase == 1 && !is_leader && player_team == session->turn_team) {
            // ANSWER|단어
            char* answer = ev.data + 7;
            bool card_found = false;
            for (int i = 0; i < MAX_CARDS; ++i) {
                if (!session->cards[i].isUsed && strcmp(session->cards[i].name, answer) == 0) {
                    card_found = true;
                    session->cards[i].isUsed = 1;
                    int t = session->cards[i].type;

                    // 카드 상태 갱신 브로드캐스트
                    char update_msg[64];
                    snprintf(update_msg, sizeof(update_msg), "CARD_UPDATE|%d|1", i);
                    broadcast_to_all(session, update_msg);

                    char chat_msg[256];
                    // 점수 반영
                    if (t == RED_CARD) {
                        if (session->turn_team == 0) {
                            session->redScore++;  // 우리 팀 카드 → 점수 추가
                            snprintf(chat_msg, sizeof(chat_msg), "CHAT|%d|%d|%s|%s",
                                    SYSTEM_TEAM, 0, SYSTEM_NAME, "레드팀 1득점");
                        } else {
                            session->remaining_tries = 0;  // 상대 팀 카드 → 턴 종료
                            session->redScore++; // 상대팀 에게 1점
                            snprintf(chat_msg, sizeof(chat_msg), "CHAT|%d|%d|%s|%s",
                                    SYSTEM_TEAM, 0, SYSTEM_NAME, "블루팀 1실점");
                        }
                        broadcast_to_all(session, chat_msg);
                    } else if (t == BLUE_CARD) {
                        if (session->turn_team == 1) {
                            session->blueScore++;  // 우리 팀 카드 → 점수 추가
                            snprintf(chat_msg, sizeof(chat_msg), "CHAT|%d|%d|%s|%s",
                                    SYSTEM_TEAM, 0, SYSTEM_NAME, "블루팀 1득점");
                        } else {
                            session->remaining_tries = 0;  // 상대 팀 카드 → 턴 종료
                            session->blueScore++;  // 상대팀에게 1점
                            snprintf(chat_msg, sizeof(chat_msg), "CHAT|%d|%d|%s|%s",
                                    SYSTEM_TEAM, 0, SYSTEM_NAME, "레드팀 1실점");
                        }
                        broadcast_to_all(session, chat_msg);
                    } else if (t == NEUTRAL_CARD) {
                        session->remaining_tries = 0;  // 민간인 카드 → 턴 종료
                        snprintf(chat_msg, sizeof(chat_msg), "CHAT|%d|%d|%s|%s",
                                    SYSTEM_TEAM, 0, SYSTEM_NAME, "민간인 카드 선택, 턴을 종료합니다");
                        broadcast_to_all(session, chat_msg);
                    } else if (t == ASSASSIN_CARD) {
                        session->game_over = true;  // 암살자 선택 → 게임 종료
                                                snprintf(chat_msg, sizeof(chat_msg), "CHAT|%d|%d|%s|%s",
                                    SYSTEM_TEAM, 0, SYSTEM_NAME, "암살자 카드 선택, 사망하셨습니다.");
                        broadcast_to_all(session, chat_msg);
                        
                        pthread_mutex_unlock(&session->game_lock);
                        goto event_gameover;
                    }
                    break;
                }
            }
 
            if (!card_found) {
                // 클라이언트에게만 잘못된 단어 알림
                int client_sock = session->init_info.players[ev.player_index].info.sock;
                char warn_msg[256];
                snprintf(warn_msg, sizeof(warn_msg), "CHAT|%d|0|%s|선택한 단어가 보드에 없습니다. 다시 입력하세요.\n",
                        SYSTEM_TEAM, SYSTEM_NAME);
                send(client_sock, warn_msg, strlen(warn_msg), 0);

                // 턴과 시도는 유지
                pthread_mutex_unlock(&session->game_lock);
                usleep(10000);
                continue;
            }

            session->remaining_tries--;
            
            char turn_msg[128];
            if (session->remaining_tries <= 0) {
                session->turn_team = 1 - session->turn_team;
                session->phase = 0;

                // 턴 전환 알림
                snprintf(turn_msg, sizeof(turn_msg), "TURN_UPDATE|%d|%d|%d|%d",
                        session->turn_team, session->phase, session->redScore, session->blueScore);

                broadcast_to_all(session, turn_msg);
            } else {
                // 점수 전송을 위해 TURN_UPDATE 재사용
                snprintf(turn_msg, sizeof(turn_msg), "TURN_UPDATE|%d|%d|%d|%d",
                        SYSTEM_TEAM, session->phase, session->redScore, session->blueScore);
                broadcast_to_all(session, turn_msg);
                
                char chat_msg[256];
                snprintf(chat_msg, sizeof(chat_msg), "CHAT|%d|0|%s|%s팀 남은 횟수 : %d",
                SYSTEM_TEAM, SYSTEM_NAME, session->turn_team == 0 ? "레드" : "블루",session->remaining_tries);
                broadcast_to_all(session, chat_msg);
            }
        } else if (ev.type == EVENT_CHAT) {
            const char* msg = ev.data + 5;
            const PlayerEntry* sender = &session->init_info.players[ev.player_index];

            char chat_msg[256];
            snprintf(chat_msg, sizeof(chat_msg), "CHAT|%d|%d|%s|%s",
                     sender->team, sender->is_leader, sender->nickname, msg);
            broadcast_to_all(session, chat_msg);
        }

        // 승리 조건 검사
        if (session->redScore >= 9 || session->blueScore >= 8) {
            session->game_over = true;
        }

        pthread_mutex_unlock(&session->game_lock);
        usleep(10000);
    }

event_gameover:
    // 게임 종료 메시지 전송
    int winner_team = (session->redScore >= 9) ? 0 :
                      (session->blueScore >= 8) ? 1 :
                      (1 - session->turn_team); // 암살자 선택 시 현재 턴팀 패배

    char end_msg[64];
    snprintf(end_msg, sizeof(end_msg), "GAME_OVER|%d", winner_team);
    broadcast_to_all(session, end_msg);

    // 여기에서 DB에 게임 결과 저장 (game_result.c)
    int result_save = 0;

    for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
        if(session->init_info.players[i].team == winner_team) {
            result_save = save_game_result_by_nickname(session->init_info.players[i].nickname, "WIN");  
        } else {
            result_save = save_game_result_by_nickname(session->init_info.players[i].nickname, "LOSS"); 
        }
    }

    // 세션 정리
    unregister_session(session);
    pthread_mutex_destroy(&session->game_lock);
    pthread_mutex_destroy(&session->event_lock);
    free(session);
    return NULL;
}

bool assign_cards(const char* filename, GameCard cards[MAX_CARDS]) {
    char raw_words[MAX_CARDS][32];
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("단어 파일 열기 실패");
        return false;
    }

    for (int i = 0; i < MAX_CARDS; ++i) {
        if (fscanf(file, "%31s", raw_words[i]) != 1) {
            fprintf(stderr, "단어 파일 형식 오류 (총 25개 필요)\n");
            fclose(file);
            return false;
        }
    }

    fclose(file);

    // 카드 타입 설정
    int types[MAX_CARDS];
    int index = 0;
    for (int i = 0; i < 9; ++i) types[index++] = RED_CARD;
    for (int i = 0; i < 8; ++i) types[index++] = BLUE_CARD;
    for (int i = 0; i < 7; ++i) types[index++] = NEUTRAL_CARD;
    types[index++] = ASSASSIN_CARD;

    // Fisher-Yates 섞기
    srand(time(NULL));
    for (int i = MAX_CARDS - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int tmp = types[i];
        types[i] = types[j];
        types[j] = tmp;
    }

    int word_used[MAX_CARDS] = {0};
    for (int i = 0; i < MAX_CARDS; ++i) {
        int w;
        do {
            w = rand() % MAX_CARDS;
        } while (word_used[w]);
        word_used[w] = 1;

        strncpy(cards[i].name, raw_words[w], sizeof(cards[i].name) - 1);
        cards[i].name[sizeof(cards[i].name) - 1] = '\0';
        cards[i].type = types[i];
        cards[i].isUsed = 0;
    }

    return true;
}

void broadcast_to_all(GameSession* session, const char* msg) {
    char full_msg[1024];
    snprintf(full_msg, sizeof(full_msg), "%s\n", msg);  // \n 붙이기
    printf("Server Response(broadcast): %s\n", full_msg);
    for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
        int sock = session->init_info.players[i].info.sock;
        if (sock > 0) {
            send(sock, full_msg, strlen(full_msg), 0);
        }
    }
}

// 게임 중인 방 세션 등록
int register_session(GameSession* session) {
    pthread_mutex_lock(&ongoing_sessions_mutex);
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (ongoing_sessions[i] == NULL) {
            ongoing_sessions[i] = session;
            pthread_mutex_unlock(&ongoing_sessions_mutex);
            return i; // 사용 가능한 session_id
        }
    }
    pthread_mutex_unlock(&ongoing_sessions_mutex);
    return -1; // 모든 슬롯이 가득 찬 경우
}

// 게임 중인 방 세션 제거
void unregister_session(GameSession* session) {
    pthread_mutex_lock(&ongoing_sessions_mutex);
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (ongoing_sessions[i] == session) {
            ongoing_sessions[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&ongoing_sessions_mutex);
}

GameSession* find_session_by_token(const char* token) {
    pthread_mutex_lock(&ongoing_sessions_mutex);
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        GameSession* session = ongoing_sessions[i];
        if (!session) continue;
        for (int j = 0; j < MAX_ROOM_PLAYERS; ++j) {
            if (strncmp(session->init_info.players[j].info.token, token, TOKEN_LEN) == 0) {
                pthread_mutex_unlock(&ongoing_sessions_mutex);
                return session;
            }
        }
    }
    pthread_mutex_unlock(&ongoing_sessions_mutex);
    return NULL;
}

bool enqueue_game_event(GameSession* session, GameEvent ev) {
    pthread_mutex_lock(&session->event_lock);

    int next_pos = (session->event_rear + 1) % MAX_EVENTS;
    if (next_pos == session->event_front) {
        // 큐가 가득 참
        pthread_mutex_unlock(&session->event_lock);
        return false;
    }

    session->event_queue[session->event_rear] = ev;
    session->event_rear = next_pos;

    //pthread_cond_signal(&session->event_cond);  // 이벤트 대기 스레드 깨우기 - 미사용
    pthread_mutex_unlock(&session->event_lock);
    return true;
}