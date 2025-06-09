#ifndef ROOM_MANAGER_H
#define ROOM_MANAGER_H

#include <pthread.h>
#include <stdbool.h>

#define MAX_ROOM_PLAYERS 6
#define TOKEN_LEN 64

// Matching Room Part
typedef enum {
    WAIT_CONTINUE,
    WAIT_GAME_STARTED,
    WAIT_GAME_ENDED,
    WAIT_CANCELLED,
    WAIT_ERROR
} WaitResult;

typedef struct {
    int sock;
    char token[TOKEN_LEN];
} PlayerInfo;

typedef struct {
    PlayerInfo players[MAX_ROOM_PLAYERS];
} MatchRoomArgs;

// 함수 선언
WaitResult handle_waiting_command(int client_sock, const char* token);
WaitResult cancel_waiting(int client_sock, const char* token);
void* run_match_room(void* arg);
void* client_response_loop(int client_sock, const char* token);


// 게임 Part
#define MAX_CARDS 25
#define RED_CARD 1
#define BLUE_CARD 2
#define NEUTRAL_CARD 3
#define ASSASSIN_CARD 4

#define SYSTEM_TEAM 2
#define SYSTEM_NAME "SYSTEM"

typedef struct {
    PlayerInfo info;
    char nickname[64];
    int role_num;
    int team;
    bool is_leader;
} PlayerEntry;

typedef struct {
    PlayerEntry players[MAX_ROOM_PLAYERS];
} GameInitInfo;

typedef struct {
    char name[32];
    int type;
    int isUsed;
} GameCard;

typedef enum {
    EVENT_NONE,
    EVENT_CHAT,
    EVENT_HINT,
    EVENT_ANSWER,
    EVENT_REPORT // 신고 기능, 미구현
} EventType;

typedef struct {
    EventType type;
    int player_index;      // GamePlayer 인덱스
    char data[128];        // HINT|word|number or ANSWER|word or CHAT|msg
} GameEvent;

#define MAX_EVENTS 64

typedef struct {
    char word_list[MAX_CARDS][32];
    GameCard cards[MAX_CARDS];
    GameInitInfo init_info;

    int redScore;
    int blueScore;
    int turn_team;        // 0: RED, 1: BLUE
    int phase;            // 0: 팀장 힌트, 1: 팀원 정답
    int remaining_tries;
    char hint_word[32];
    int hint_count;

    bool game_over;
    pthread_mutex_t game_lock;

    // Event queue
    GameEvent event_queue[MAX_EVENTS];
    int event_front;
    int event_rear;
    pthread_mutex_t event_lock;
    pthread_cond_t event_cond;
} GameSession;

void* run_game_session(void* arg);
bool assign_cards(const char* filename, GameCard cards[MAX_CARDS]);
void broadcast_to_all(GameSession* session, const char* msg);
GameSession* find_session_by_token(const char* token);
int register_session(GameSession* session);
void unregister_session(GameSession* session);
bool enqueue_game_event(GameSession* session, GameEvent ev);

#endif
