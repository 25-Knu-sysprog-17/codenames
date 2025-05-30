#ifndef SESSION_H
#define SESSION_H

int add_token_to_db(const char *user_id, const char *token);
int is_valid_token_in_db(const char *token);
int remove_token_from_db(const char *token);

#endif
