#ifndef WAITING_SCREEN_H
#define WAITING_SCREEN_H

#include <pthread.h>
#include <stdbool.h>

/// @brief 대기 상태 결과를 나타내는 열거형
typedef enum {
    WAIT_SUCCESS,   ///< 매칭 또는 작업이 성공적으로 완료됨
    WAIT_CANCELED,  ///< 사용자가 Q 키를 눌러 작업을 취소함
    WAIT_FAILED     ///< 작업 도중 오류가 발생하여 실패
} WaitingResult;

/// @brief 외부에서 진행률 상태를 제어하기 위한 공유 상태 구조체
typedef struct {
    int progress;           ///< 현재 진행률 (0~100)
    bool done;              ///< 작업 완료 여부
    bool success;           ///< 작업의 성공 여부
    pthread_mutex_t lock;   ///< 상태 동기화를 위한 뮤텍스
} TaskStatus;

/// @brief 외부 스레드 기반의 비동기 작업을 진행하면서 대기 화면을 출력합니다.
/// @param status 진행률과 완료 여부를 공유하기 위한 상태 구조체 포인터
/// @param thread_func 별도의 스레드에서 실행할 함수 포인터 (인자로 TaskStatus*를 받아야 함)
/// @return WaitingResult: 완료, 취소 또는 실패 결과를 반환
WaitingResult waiting(TaskStatus* status, void* (*thread_func)(void*));

/// @brief TaskStatus 상태를 원자적으로 업데이트합니다.
/// @param status TaskStatus 포인터
/// @param progress 진행률 (0~100)
/// @param done 완료 여부
/// @param success 성공 여부
void update_task_status(TaskStatus* status, int progress, bool done, bool success);

/// @brief 대기 화면 상단에 표시되는 메시지를 설정합니다.
/// @param new_msg 화면에 출력할 문자열 메시지
void set_waiting_message(const char* new_msg);

/// @brief 사용자의 Q 키 입력을 통한 취소 가능 여부를 설정합니다.
/// @param enabled true면 취소 가능, false면 취소 불가
void set_cancel_enabled(bool enabled);

#endif // WAITING_SCREEN_H