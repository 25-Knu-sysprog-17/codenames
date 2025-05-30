#ifndef RESULT_SCREEN_H
#define RESULT_SCREEN_H

#include <ncurses.h>
#include <string.h>

typedef enum {
    RESULT_WIN,
    RESULT_LOSE,
} Result;

// 결과 화면 출력 함수
void result_function(int red_score, int blue_score, Result result);

#endif
