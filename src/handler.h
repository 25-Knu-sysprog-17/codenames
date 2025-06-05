#ifndef HANDLER_H
#define HANDLER_H

#include <sqlite3.h>

extern const char *DB_NAME;
extern sqlite3 *db;

// DB 초기화 함수 (테이블 생성 등)
int init_db(void);

// DB 연결 함수
int db_open(sqlite3 **db);

// DB 연결 종료 함수
void db_close(sqlite3 *db);

#endif
