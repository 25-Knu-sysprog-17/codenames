#include "handler.h"
#include <stdio.h>
#include <sqlite3.h>

const char *DB_NAME = "../db/user.db";
const char *create_table_sql =
    "CREATE TABLE IF NOT EXISTS users ("
    "id TEXT PRIMARY KEY,"
    "pw TEXT NOT NULL,"
    "salt TEXT NOT NULL,"
    "nickname TEXT UNIQUE NOT NULL,"
    "report_count INTEGER DEFAULT 0,"
    "is_suspended INTEGER DEFAULT 0);"
    "CREATE TABLE IF NOT EXISTS game_stats ("
    "user_id TEXT PRIMARY KEY,"
    "wins INTEGER DEFAULT 0,"
    "losses INTEGER DEFAULT 0,"
    "FOREIGN KEY(user_id) REFERENCES users(id));"
    "CREATE TABLE IF NOT EXISTS sessions ("
    "user_id TEXT NOT NULL,"
    "token TEXT PRIMARY KEY,"
    "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
    "FOREIGN KEY(user_id) REFERENCES users(id));";

// DB 초기화
int init_db(void) { 
    sqlite3 *db; 
    char *err_msg = NULL; 
    int rc = sqlite3_open(DB_NAME, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "DB open error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_busy_timeout(db, 5000); // 잠금 대기 시간 설정

    rc = sqlite3_exec(db, create_table_sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) { // 에러 처리
        fprintf(stderr, "DB exec error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }
    sqlite3_close(db);
    return 0;
}

int db_open(sqlite3 **db) {
    int rc = sqlite3_open(DB_NAME, db); // DB 연결 시도
    if (rc != SQLITE_OK) {
        fprintf(stderr, "DB open error: %s\n", sqlite3_errmsg(*db));
        return -1;
    }
    sqlite3_busy_timeout(*db, 5000);
    return 0;
}

void db_close(sqlite3 *db) {
    if (db) sqlite3_close(db); // db가 NULL이 아니면 닫음
}
