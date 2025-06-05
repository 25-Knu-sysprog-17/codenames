#include "user_report.h"
#include "handler.h"
#include <stdio.h>
#include <sqlite3.h>

// 사용자를 신고하고, 신고 횟수가 3번 이상이면 자동으로 계정 정지
int report_user(const char *id) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    char *errmsg = NULL;

    // 트랜잭션 시작
    if (sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &errmsg) != SQLITE_OK) {
        sqlite3_free(errmsg);
        db_close(db);
        return -1;
    }

    int result = -1;
    sqlite3_stmt *stmt = NULL;

    // 1. 신고 횟수 증가
    const char *sql = "UPDATE users SET report_count = report_count + 1 WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) goto rollback;
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) goto rollback;
    sqlite3_finalize(stmt);

    // 2. 신고 횟수 조회
    sql = "SELECT report_count FROM users WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) goto rollback;
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    int report_count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        report_count = sqlite3_column_int(stmt, 0);
    } else {
        goto rollback;
    }
    sqlite3_finalize(stmt);

    // 3. 정지 처리
    if (report_count >= 3) {
        sql = "UPDATE users SET is_suspended = 1 WHERE id = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) goto rollback;
        sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE) goto rollback;
        sqlite3_finalize(stmt);
    }

    // 트랜잭션 커밋
    if (sqlite3_exec(db, "COMMIT;", NULL, NULL, &errmsg) != SQLITE_OK) {
        sqlite3_free(errmsg);
        goto rollback_final;
    }

    result = report_count;
    goto final; // 성공

rollback: // 중간 어딘가에서 실패
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);

rollback_final:
    result = -1;

final:
    db_close(db);
    return result;
}


// 사용자 계정을 강제로 정지시키는 함수
int suspend_user(const char *id) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE users SET is_suspended = 1 WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        db_close(db);
        return -1;
    }
    sqlite3_finalize(stmt);
    db_close(db);
    return 0;
}

// 유저가 정지 상태인지 확인하는 함수
int is_account_suspended(const char *id) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT is_suspended FROM users WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    int suspended = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        suspended = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    db_close(db);
    return suspended;
}

// 유저가 지금까지 몇 번 신고당했는지 조회하는 함수
int get_report_count(const char *id) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT report_count FROM users WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    int report_count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        report_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    db_close(db);
    return report_count;
}
