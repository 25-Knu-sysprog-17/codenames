#ifndef LOBBY_SCREEN_H
#define LOBBY_SCREEN_H

#include "waiting_screen.h"  // waiting() 및 WaitingResult 사용 시 필요

/// @brief 유저 정보를 담는 구조체
typedef struct {
    char nickname[32];
    int wins;
    int losses;
} UserInfo;

/// @brief 로비 화면을 출력하고 입력을 처리함
/// @param user 로그인한 유저 정보 포인터
void lobby_screen(const UserInfo* user);

#endif // LOBBY_SCREEN_H