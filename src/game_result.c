#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "handler.h"
#include "user_info.h"
#include "session.h"

int get_user_id_by_nickname2(const char *nickname, char *user_id, size_t size) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    const char *sql = "SELECT id FROM users WHERE nickname = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    if (sqlite3_bind_text(stmt, 1, nickname, -1, SQLITE_STATIC) != SQLITE_OK) {
        fprintf(stderr, "Bind error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *id = sqlite3_column_text(stmt, 0);
        strncpy(user_id, (const char *)id, size);
        result = 0;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

int save_game_result_by_nickname(const char *nickname, const char *result) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    char user_id[128];
    if (get_user_id_by_nickname2(nickname, user_id, sizeof(user_id)) != 0) {
        sqlite3_close(db);
        return -1; // 닉네임이 존재하지 않음
    }

    const char *sql_win = "INSERT INTO game_stats(user_id, wins, losses) VALUES(?, 1, 0) "
                          "ON CONFLICT(user_id) DO UPDATE SET wins = wins + 1;";
    const char *sql_loss = "INSERT INTO game_stats(user_id, wins, losses) VALUES(?, 0, 1) "
                           "ON CONFLICT(user_id) DO UPDATE SET losses = losses + 1;";

    const char *sql = NULL;
    if (strcmp(result, "WIN") == 0) {
        sql = sql_win;
    } else if (strcmp(result, "LOSS") == 0) {
        sql = sql_loss;
    } else {
        sqlite3_close(db);
        return -1; // 잘못된 결과 값
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    if (sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC) != SQLITE_OK) {
        fprintf(stderr, "SQL bind error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return -1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
