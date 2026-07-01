#pragma once

#include "sequence/SequenceContext.h"
#include <string_view>
#include <chrono>

// 설계서 5.1.1: 각 시퀀스 단계는 시작 조건, 완료 조건, 타임아웃, 실패 시 복구 동작을 포함한다.
// 단계는 재진입 가능성과 중단 가능성을 고려해 설계한다.
namespace rs::sequence
{
    enum class StepResult
    {
        Running,     // 진행 중 — 다음 Tick 에서 계속 Execute
        Completed,   // 완료 — 다음 단계로
        Failed       // 실패 — OnFailure 후 시퀀스 중단
    };

    class ISequenceStep
    {
    public:
        virtual ~ISequenceStep() = default;

        virtual std::wstring_view Name() const noexcept = 0;

        virtual bool CanStart(SequenceContext& ctx) = 0;        // 시작 조건
        virtual void OnEnter(SequenceContext& ctx) = 0;         // 진입 처리
        virtual StepResult Execute(SequenceContext& ctx) = 0;   // 주기 호출, 완료 조건 판단
        virtual void OnExit(SequenceContext& ctx) = 0;          // 정상/중단 종료 처리
        virtual void OnFailure(SequenceContext& ctx) = 0;       // 실패 복구 동작

        virtual std::chrono::milliseconds Timeout() const noexcept = 0;
        virtual bool CanPause() const noexcept = 0;             // 7.1.8 Step별 Pause 허용
    };
}
