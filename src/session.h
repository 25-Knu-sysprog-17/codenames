#ifndef SESSION_H
#define SESSION_H

int add_token_to_db_after_removal(const char *user_id, const char *token);
int is_valid_token_in_db(const char *token);
int remove_token_from_db(const char *token);
int get_user_id_by_token(const char *token, char *user_id, int user_id_len);
int get_user_id_by_nickname(const char *nickname, char *out_user_id);

#endif
