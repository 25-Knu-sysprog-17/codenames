#include "matchmaking_server.h"
#include <string.h>
#include <arpa/inet.h>

static ClientInfo client_list[MAX_CLIENTS];  // 전체 연결 목록
static int matching_queue[MAX_PLAYERS];      // 매칭 큐 (인덱스 보관)
static int queue_len = 0;

void init_client_queue() {
    queue_len = 0;
    memset(matching_queue, -1, sizeof(matching_queue));
    memset(client_list, 0, sizeof(client_list));
}

int find_available_client_slot() {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (client_list[i].sock == 0)
            return i;
    }
    return -1;
}

void store_client(int index, ClientInfo client) {
    client_list[index] = client;
}

int add_to_matching_queue(int index) {
    if (queue_len >= MAX_PLAYERS) return 0;
    matching_queue[queue_len++] = index;
    return 1;
}

int matching_queue_ready() {
    return queue_len == MAX_PLAYERS;
}

int get_matching_queue_count() {
    return queue_len;
}

ClientInfo* get_client_by_index(int index) {
    if (index < 0 || index >= MAX_CLIENTS) return NULL;
    return &client_list[index];  // client_list[]는 ClientInfo 배열
}

void get_match_room(MatchRoomArgs* room) {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        int idx = matching_queue[i];
        room->player_sock[i] = client_list[idx].sock;
        inet_ntop(AF_INET, &client_list[idx].addr.sin_addr, room->player_ip[i], INET_ADDRSTRLEN);
        room->player_port[i] = ntohs(client_list[idx].addr.sin_port);
    }

    room->team_leader[0] = 0;
    room->team_members[0][0] = 1;
    room->team_members[0][1] = 2;
    room->team_leader[1] = 3;
    room->team_members[1][0] = 4;
    room->team_members[1][1] = 5;

    queue_len = 0;
}
