#pragma once

#include "core/domain/InterlockClass.h"
#include <cstdint>
#include <string>
#include <functional>

// 설계서 v2 별첨 A.7: InterlockRule — 인터락 조건, 감시 대상, 트리거 조건, 복구 조건 정의.
// InterlockManager 가 분류별 평가 주기(설계서 5.6.5)에 따라 평가한다.
namespace rs::domain
{
    // 평가 위치/주기 힌트(설계서 5.6.5). Safety 는 실시간 루프, Operation 은 명령 실행 전 검증.
    enum class InterlockEvalCycle
    {
        RealTimeLoop,   // EMO, Door, Limit — 최상 우선순위
        Periodic,       // Vacuum, Pressure 등 10~100ms
        PreCommand,     // Recipe, Permission — 명령 실행 전
        EventDriven     // Host Online 상태 등
    };

    struct InterlockRule
    {
        std::wstring        id;                 // 규칙 식별자 (예: L"ILK-DOOR-01")
        std::wstring        description;        // 표시/로그용 설명
        InterlockClass      classification{ InterlockClass::Operation };
        InterlockEvalCycle  evalCycle{ InterlockEvalCycle::PreCommand };
        std::uint32_t       alarmOnViolation{ 0 }; // 위반 시 발생시킬 알람 코드(AlarmCode.h 의 범위 체계 사용)
        bool                maskable{ false };  // 마스킹 허용 여부 — Safety 는 항상 false(설계서 5.6.8)

        // 조건 평가자: true = 조건 만족(안전), false = 위반.
        // 실시간 루프용 규칙의 평가자는 블로킹/동적 할당 없이 작성한다(설계서 6.5.3).
        std::function<bool()> evaluate;

        // 복구 조건: 위반 해소 판정. 미지정 시 evaluate 재평가로 대체.
        std::function<bool()> isRecovered;
    };
}
