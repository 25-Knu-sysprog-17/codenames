#include "matchmaking_server.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>

void* run_match_room(void* arg) {
    MatchRoomArgs* room = (MatchRoomArgs*)arg;

    // 예시: 단순히 각 유저에게 메시지를 보내고 종료
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        char msg[] = "✅ 게임 방에 입장하였습니다!";
        send(room->player_sock[i], msg, strlen(msg), 0);
        close(room->player_sock[i]);  // 예시로 소켓 종료 (실제 게임 서버에서는 유지)
    }

    free(room);
    pthread_exit(NULL);
}
