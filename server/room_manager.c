
#include "matchmaking_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_ROOM_PLAYERS 6

static MatchRoomArgs* current_room = NULL;

void broadcast_to_room(const char* msg, int except_sock) {
    for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
        int sock = current_room->player_sock[i];
        if (sock > 0 && sock != except_sock) {
            send(sock, msg, strlen(msg), 0);
        }
    }
}

void* room_client_thread(void* arg) {
    int sock = *(int*)arg;
    free(arg);

    char buf[256];
    while (1) {
        int bytes = recv(sock, buf, sizeof(buf) - 1, 0);
        if (bytes <= 0) break;

        buf[bytes] = '\0';

        if (strncmp(buf, "CHAT|", 5) == 0) {
            // 그대로 모든 클라이언트에 전송
            broadcast_to_room(buf, sock);
        }
    }

    close(sock);
    return NULL;
}

void* run_match_room(void* arg) {
    current_room = (MatchRoomArgs*)arg;

    printf("🎮 게임 방 시작: 플레이어 %d명\n", MAX_ROOM_PLAYERS);

    for (int i = 0; i < MAX_ROOM_PLAYERS; ++i) {
        int* sock = malloc(sizeof(int));
        *sock = current_room->player_sock[i];
        pthread_t tid;
        pthread_create(&tid, NULL, room_client_thread, sock);
        pthread_detach(tid);
    }

    // 게임 방 종료 로직 생략 (비동기 구조)
    free(current_room);
    pthread_exit(NULL);
}
