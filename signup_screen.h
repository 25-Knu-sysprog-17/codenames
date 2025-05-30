#ifndef SIGNUP_SCREEN_H
#define SIGNUP__SCREEN_H

#include <ncurses.h>
#include <string.h>
#include <signal.h>

#define MAX_INPUT 32

typedef enum {
    STATE_ID_INPUT,
    STATE_PW_INPUT,
    STATE_NICKNAME_INPUT,
    STATE_ID_CHECK,
    STATE_NICKNAME_CHECK,
    STATE_SIGNUP_BUTTON
} UIState;

// 각각 id, pw, nickname 배열에 결과 저장.
void signup_screen(char *id, char *pw, char *nickname);

#endif
