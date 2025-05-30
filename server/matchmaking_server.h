#ifndef MATCHMAKING_SERVER_H
#define MATCHMAKING_SERVER_H

#include <netinet/in.h>

#define MAX_PLAYERS 6
#define MAX_CLIENTS 100

typedef struct {
    int sock;
    struct sockaddr_in addr;
} ClientInfo;

typedef struct {
    int player_sock[MAX_PLAYERS];
    char player_ip[MAX_PLAYERS][INET_ADDRSTRLEN];
    int player_port[MAX_PLAYERS];
    int team_leader[2];
    int team_members[2][2];
} MatchRoomArgs;

void init_client_queue();
int add_to_matching_queue(int index);
int matching_queue_ready();
void get_match_room(MatchRoomArgs* room);
int get_matching_queue_count();
int find_available_client_slot();
void store_client(int index, ClientInfo client);
ClientInfo* get_client_by_index(int index);

void* run_match_room(void* arg);

#endif
