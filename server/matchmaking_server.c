
#include "matchmaking_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_PORT 4000

void* handle_client_thread(void* arg) {
    int index = *(int*)arg;
    free(arg);

    ClientInfo* client = get_client_by_index(index);
    int sock = client->sock;

    char buffer[256];
    while (1) {
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            close(sock);
            client->sock = 0;
            break;
        }

        buffer[bytes] = '\0';

        // 매칭 전 요청만 처리
        char* type = strtok(buffer, "|");
        if (!type) continue;

        if (strcmp(type, "CMD") == 0) {
            char* cmd = strtok(NULL, "\n");
            if (cmd && strcmp(cmd, "QUERY_WAIT") == 0) {
                int count = get_matching_queue_count();
                char reply[32];
                snprintf(reply, sizeof(reply), "WAIT_REPLY|%d", count);
                send(sock, reply, strlen(reply), 0);
            }
        }

        // CHAT 등은 이후 room_manager에서 처리하므로 여기선 무시
    }

    return NULL;
}

int main() {
    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    init_client_queue();

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, MAX_PLAYERS);

    printf("🔁 매치메이킹 서버 실행 중 (포트 %d)...\n", SERVER_PORT);

    while (1) {
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) continue;

        int index = find_available_client_slot();
        if (index == -1) {
            printf("❌ 클라이언트 슬롯 부족\n");
            close(client_sock);
            continue;
        }

        ClientInfo new_client = { client_sock, client_addr };
        store_client(index, new_client);
        add_to_matching_queue(index);

        printf("🟢 클라이언트 접속: %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_t tid;
        int* arg = malloc(sizeof(int));
        *arg = index;
        pthread_create(&tid, NULL, handle_client_thread, arg);
        pthread_detach(tid);

        if (matching_queue_ready()) {
            MatchRoomArgs* args = malloc(sizeof(MatchRoomArgs));
            get_match_room(args);

            pthread_t room_tid;
            pthread_create(&room_tid, NULL, run_match_room, (void*)args);
            pthread_detach(room_tid);
        }
    }

    close(server_sock);
    return 0;
}
