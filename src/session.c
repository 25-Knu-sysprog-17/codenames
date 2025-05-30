#include "session.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

extern const char *DB_NAME;

// 토큰을 DB에 추가
int add_token_to_db(const char *user_id, const char *token) {
    sqlite3 *db;
    if (sqlite3_open(DB_NAME, &db) != SQLITE_OK) {
        fprintf(stderr, "DB open error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "INSERT INTO sessions(user_id, token) VALUES ('%s', '%s');", user_id, token);

    char *err_msg = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }
    sqlite3_close(db);
    return 0;
}

// 토큰이 유효한지 DB에서 확인
int is_valid_token_in_db(const char *token) {
    sqlite3 *db;
    if (sqlite3_open(DB_NAME, &db) != SQLITE_OK) {
        fprintf(stderr, "DB open error: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT user_id FROM sessions WHERE token = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);

    int result = (sqlite3_step(stmt) == SQLITE_ROW) ? 1 : 0;
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

// 토큰을 DB에서 삭제
int remove_token_from_db(const char *token) {
    sqlite3 *db;
    if (sqlite3_open(DB_NAME, &db) != SQLITE_OK) {
        fprintf(stderr, "DB open error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM sessions WHERE token = '%s';", token);

    char *err_msg = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }
    sqlite3_close(db);
    return 0;
}
