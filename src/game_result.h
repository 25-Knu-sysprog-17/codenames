#ifndef GAME_RESULT_H
#define GAME_RESULT_H

// 게임 결과를 DB에 저장하는 함수
// token: 사용자 토큰
// result: "WIN" 또는 "LOSS"
// 성공 시 0, 실패 시 -1 반환
int save_game_result_by_nickname(const char *nickname, const char *result);

#endif // GAME_RESULT_H