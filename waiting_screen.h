#ifndef WAITING_SCREEN_H
#define WAITING_SCREEN_H

#include <ncurses.h>
#include <unistd.h>
#include <string.h>

#define TOTAL_PLAYERS 20

// 전체 로딩 화면 함수
void waiting(bool (*task_callback)(int* progress_out));
void set_waiting_message(const char* new_msg);

#endif // LOADING_SCREEN_H
