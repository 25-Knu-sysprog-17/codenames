#include <stdio.h>
#include <string.h>
#include "handler.h"
#include "session.h" // get_user_id_by_token 정의
#include "user_info.h"

int get_user_info_by_token(const char *token, char *user_id, int user_id_len, char *nickname, int nickname_len, int *wins, int *losses) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    if (get_user_id_by_token(token, user_id, user_id_len) != 0) {
        sqlite3_close(db);
        return -1; // 토큰이 유효하지 않음
    }

    printf("id : %s\n", user_id);

    sqlite3_stmt *stmt;

    // 닉네임 조회
    const char *sql_nick = "SELECT nickname FROM users WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql_nick, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL error (prepare nickname): %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        strncpy(nickname, (const char*)sqlite3_column_text(stmt, 0), nickname_len - 1);
        nickname[nickname_len - 1] = '\0';
    } else {
        fprintf(stderr, "No nickname found for user_id: %s\n", user_id);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }
    sqlite3_finalize(stmt);
    printf("nickname : %s\n", nickname);

    // wins, losses 조회
    const char *sql_stats = "SELECT wins, losses FROM game_stats WHERE user_id = ?;";
    if (sqlite3_prepare_v2(db, sql_stats, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL error (prepare stats): %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *wins = sqlite3_column_int(stmt, 0);
        *losses = sqlite3_column_int(stmt, 1);
    } else {
        *wins = 0;
        *losses = 0;
    }
    sqlite3_finalize(stmt);

    printf("win: %d lose: %d\n", *wins, *losses);

    sqlite3_close(db);
    return 0;
}

int get_nickname_by_token(const char *token, char *nickname, int nickname_len) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    char user_id[64];
        if (get_user_id_by_token(token, user_id, sizeof(user_id)) != 0) {
        sqlite3_close(db);
        return -1; // 토큰 유효하지 않음
    }

    sqlite3_stmt *stmt;
    const char* sql = "SELECT nickname FROM users WHERE id = ?";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL error (prepare nickname): %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC);

    int result = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *nick = sqlite3_column_text(stmt, 0);
        strncpy(nickname, (const char *)nick, nickname_len - 1);
        nickname[nickname_len - 1] = '\0';
        result = 0;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

