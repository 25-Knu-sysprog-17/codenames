#ifndef USER_REPORT_H
#define USER_REPORT_H

int report_user(const char *id);
int suspend_user(const char *id);
int is_account_suspended(const char *id);
int get_report_count(const char *id);

#endif
