#include "session.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

extern const char *DB_NAME;

// 기존 토큰 지우고 토큰 DB에 저장
int add_token_to_db_after_removal(const char *user_id, const char *token) {
    sqlite3 *db;
    if (sqlite3_open(DB_NAME, &db) != SQLITE_OK) {
        fprintf(stderr, "DB open error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // 트랜잭션 시작 (동시성 문제 방지)
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    // 기존 토큰 삭제
    char del_sql[256];
    snprintf(del_sql, sizeof(del_sql), "DELETE FROM sessions WHERE user_id = '%s';", user_id);
    char *err_msg = NULL;
    if (sqlite3_exec(db, del_sql, NULL, NULL, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_close(db);
        return -1;
    }

    // 새 토큰 추가
    char ins_sql[256];
    snprintf(ins_sql, sizeof(ins_sql), "INSERT INTO sessions(user_id, token) VALUES ('%s', '%s');", user_id, token);
    if (sqlite3_exec(db, ins_sql, NULL, NULL, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_close(db);
        return -1;
    }

    // 트랜잭션 커밋
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);

    sqlite3_close(db);
    return 0;
}





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

// 토큰으로 user_id 가져오기
int get_user_id_by_token(const char *token, char *user_id, int user_id_len) {
    sqlite3 *db;
    if (sqlite3_open(DB_NAME, &db) != SQLITE_OK) {
        fprintf(stderr, "DB open error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT user_id FROM sessions WHERE token = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *id = (const char*)sqlite3_column_text(stmt, 0);
        strncpy(user_id, id, user_id_len - 1);
        user_id[user_id_len - 1] = '\0';
        result = 0;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

int get_user_id_by_nickname(const char* nickname, char *out_user_id) {
    sqlite3 *db;
    if (sqlite3_open(DB_NAME, &db) != SQLITE_OK) {
        fprintf(stderr, "DB open error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char* sql = "SELECT id FROM users WHERE nickname = ?";
    sqlite3_stmt *stmt;
    int result = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, nickname, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *id_text = sqlite3_column_text(stmt,0);
            if (id_text) {
                strncpy(out_user_id, (const char*) id_text, 63);
                out_user_id[63] = '\0';
                result = 0; //성공
            }
        }

        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return result;
}