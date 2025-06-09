#ifndef USER_AUTH_H
#define USER_AUTH_H

#define SALT_LEN 16
#define HASH_LEN 65

// 솔트 및 해싱
void generate_salt(char *salt, int len);
void hash_password_with_salt(const char *pw, const char *salt, char *hashed_pw);

// 사용자 인증 관련 기능
int check_id_duplicate(const char *id);
int check_nickname_duplicate(const char *nickname);
int signup_user(const char *id, const char *pw, const char *nickname);
int login_user_db(const char *id, const char *pw);
int change_password(const char *id, const char *new_pw);
int withdraw_user(const char *id);
int change_nickname(const char *id, const char *new_nickname);
void generate_token(char *token, int len);

#endif
