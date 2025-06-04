#ifndef USER_INFO_H
#define USER_INFO_H

#include <sqlite3.h>

extern sqlite3 *db;

// 토큰으로 user_id, nickname, wins, losses를 한 번에 반환
int get_user_info_by_token(const char *token, char *user_id, int user_id_len, char *nickname, int nickname_len, int *wins, int *losses);

#endif
