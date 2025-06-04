#include "connect_to_server.h"
#include "waiting_screen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

static int server_socket = -1;

// 서버 소켓 반환
int get_server_socket() {
    return server_socket;
}

// 서버 연결
bool connect_to_server(const char* ip, int port) {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) return false;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        server_socket = -1;
        return false;
    }

    return true;
}

// 연결 성공 여부
bool server_connection_successful() {
    return server_socket > 0;
}

// 매칭 대기 상태 확인 (CMD|QUERY_WAIT 사용)
bool wait_for_match(int* progress_out) {
    static int last_count = 0;
    set_waiting_message("매칭 대기 중...");

    int sock = get_server_socket();
    if (sock <= 0) return true;

    const char* cmd = "CMD|QUERY_WAIT";  // ✅ 프로토콜 변경
    send(sock, cmd, strlen(cmd), 0);

    char buf[64] = {0};
    int bytes = recv(sock, buf, sizeof(buf) - 1, 0);
    if (bytes <= 0) {
        set_waiting_message("서버 응답 없음");
        *progress_out = 100;
        return true;
    }

    buf[bytes] = '\0';

    if (strncmp(buf, "WAIT_REPLY|", 11) == 0) {
        int count = atoi(buf + 11);
        *progress_out = count * 100 / 6;
        if (count >= 6) {
            set_waiting_message("매칭 완료! 게임 시작...");
            return true;
        }
    } else {
        set_waiting_message("❌ 서버 응답 오류");
        *progress_out = 100;
        return true;
    }

    return false;
}
