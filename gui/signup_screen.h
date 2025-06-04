#ifndef SIGNUP_SCREEN_H
#define SIGNUP__SCREEN_H

#include <ncurses.h>
#include <string.h>
#include <signal.h>

#define SIGN_MAX_INPUT 32

SceneState signup_screen(char *id, char* pw, char* nickname);

#endif
