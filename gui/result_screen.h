#ifndef RESULT_SCREEN_H
#define RESULT_SCREEN_H

#include "codenames_screen.h"
#include <ncurses.h>
#include <string.h>

// 결과 화면 출력 함수
SceneState result_function(int red_score, int blue_score, Result result, int winner_team);

#endif
