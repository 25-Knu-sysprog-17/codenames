#include "user_auth.h"
#include "handler.h"
#include "session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <openssl/sha.h>
#include <time.h>

#define SALT_LEN 16
#define HASH_LEN 65
#define TOKEN_LEN 32

// 솔트 생성 -> 비밀번호 해시 시 사용
void generate_salt(char *salt, int len) { 
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < len - 1; ++i) {
        salt[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    salt[len - 1] = '\0';
}

// 비밀번호 암호화
void hash_password_with_salt(const char *pw, const char *salt, char *hashed_pw) {
    char salted_pw[256];
    snprintf(salted_pw, sizeof(salted_pw), "%s%s", pw, salt);
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)salted_pw, strlen(salted_pw), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(hashed_pw + (i * 2), "%02x", hash[i]);
    }
    hashed_pw[64] = '\0';
}

// 토큰 생성
void generate_token(char *token, int len) {
    const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    srand(time(NULL));
    for (int i = 0; i < len - 1; i++) {
        token[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    token[len - 1] = '\0';
}

// 아이디 중복 체크
int check_id_duplicate(const char *id) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc = sqlite3_open(DB_NAME, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "DB open error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    const char *sql = "SELECT id FROM users WHERE id = ?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    int is_duplicate = (rc == SQLITE_ROW) ? 1 : 0;
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return is_duplicate;
}

// 닉네임 중복 체크
int check_nickname_duplicate(const char *nickname) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc = sqlite3_open(DB_NAME, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "DB open error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    const char *sql = "SELECT nickname FROM users WHERE nickname = ?;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, nickname, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    int is_duplicate = (rc == SQLITE_ROW) ? 1 : 0;
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return is_duplicate;
}


// 회원가입
int signup_user(const char *id, const char *pw, const char *nickname) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    // 중복 재체크
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id FROM users WHERE id = ? OR nickname = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, nickname, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        db_close(db);
        return -2; // 중복
    }
    sqlite3_finalize(stmt);

    // salt 생성 및 비밀번호 해싱
    char salt[SALT_LEN];
    generate_salt(salt, SALT_LEN);
    char hashed_pw[HASH_LEN];
    hash_password_with_salt(pw, salt, hashed_pw);

    // 회원가입
    sql = "INSERT INTO users(id, pw, salt, nickname, report_count, is_suspended) VALUES (?, ?, ?, ?, 0, 0);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hashed_pw, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, salt, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, nickname, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        db_close(db);
        return -1;
    }
    sqlite3_finalize(stmt);
    db_close(db);
    return 0;
}

// 로그인
int login_user_db(const char *id, const char *pw) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT pw, salt FROM users WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        db_close(db);
        return -2; // 없는 계정
    }
    const unsigned char *stored_pw = sqlite3_column_text(stmt, 0);
    const unsigned char *salt = sqlite3_column_text(stmt, 1);
    char hashed_pw[HASH_LEN];
    hash_password_with_salt(pw, (const char *)salt, hashed_pw);
    int result = strcmp((const char *)stored_pw, hashed_pw) == 0 ? 0 : -3; // 비밀번호 불일치
    sqlite3_finalize(stmt);
    db_close(db);
    return result;
}

// 비밀번호 수정
int change_password(const char *id, const char *new_pw) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    // salt 재사용 또는 새로 생성 가능 (여기선 기존 salt 사용)
    sqlite3_stmt *stmt;
    const char *sql = "SELECT salt FROM users WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        db_close(db);
        return -2; // 없는 계정
    }
    const unsigned char *salt = sqlite3_column_text(stmt, 0);
    char hashed_pw[HASH_LEN];
    hash_password_with_salt(new_pw, (const char *)salt, hashed_pw);
    sqlite3_finalize(stmt);

    sql = "UPDATE users SET pw = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, hashed_pw, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, id, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        db_close(db);
        return -1;
    }
    sqlite3_finalize(stmt);
    db_close(db);
    return 0;
}

// 사용자 삭제
int withdraw_user(const char *id) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM users WHERE id = ?;";
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

// 닉네임 수정
int change_nickname(const char *id, const char *new_nickname) {
    sqlite3 *db;
    if (db_open(&db) != 0) return -1;

    // 닉네임 중복 체크
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id FROM users WHERE nickname = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, new_nickname, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        db_close(db);
        return -2; // 중복
    }
    sqlite3_finalize(stmt);

    sql = "UPDATE users SET nickname = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, new_nickname, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, id, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        db_close(db);
        return -1;
    }
    sqlite3_finalize(stmt);
    db_close(db);
    return 0;
}
