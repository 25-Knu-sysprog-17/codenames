#ifndef CODENAMES_SCREEN_H
#define CODENAMES_SCREEN_H

#define MAX_CARDS 25
#define CHAT_LOG_SIZE 100

#include <stdbool.h>

typedef struct {
    int session_id;
    char nickname[64];
    int role_num;
    int team;
    bool is_leader;
} PlayerEntry;

typedef struct {
    PlayerEntry players[6];
    int myPlayerIndex;
} GameInitInfo;

SceneState codenames_screen(GameInitInfo init_info);

#endif