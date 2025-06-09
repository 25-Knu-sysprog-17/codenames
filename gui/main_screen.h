#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H

#include <ncurses.h>
#include <string.h>
#include <math.h>

#define TOKEN_LEN 64

// UserInfo 구조체
typedef struct {
    char nickname[32];
    int wins;
    int losses;
} UserInfo;

// 테두리 그리기 함수
void draw_border(void);

// 메인 화면 그리기 함수
void draw_main_screen(UserInfo *info);

// 메인 화면 UI 함수
SceneState main_screen_ui(void);

#endif
