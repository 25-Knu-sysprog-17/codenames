#ifndef WAITING_SCREEN_H
#define WAITING_SCREEN_H

#include <stdbool.h>

/// @brief 대기 상태 결과
typedef enum {
    WAIT_SUCCESS,   // 매칭 완료
    WAIT_CANCELED,  // 사용자가 Q를 눌러 취소
    WAIT_FAILED     // 예비 실패 처리용
} WaitingResult;

/// @brief 대기 화면 표시 및 진행 확인
/// @param task_callback 현재 진행률을 전달하는 콜백 함수
/// @return WaitingResult: 완료, 취소, 실패
WaitingResult waiting(bool (*task_callback)(int* progress_out));

/// @brief 상단 메시지 텍스트 설정
/// @param new_msg 문자열 메시지
void set_waiting_message(const char* new_msg);

#endif // WAITING_SCREEN_H