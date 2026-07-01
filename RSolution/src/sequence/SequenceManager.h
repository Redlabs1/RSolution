#pragma once

#include "sequence/SequenceContext.h"
#include "sequence/ISequenceStep.h"
#include "sequence/TimeoutGuard.h"
#include "core/common/Result.h"

#include <vector>
#include <memory>
#include <atomic>
#include <optional>
#include <cstddef>
#include <string_view>

// 설계서 5.1.1: 자동 운전 절차 총괄. 단계를 순차 실행하며 타임아웃/정지/중단/일시정지를 관리한다.
// 상태 전이는 반드시 EquipmentStateMachine 단일 진입점을 경유한다(7.1.5).
namespace rs::sequence
{
    class SequenceManager
    {
    public:
        explicit SequenceManager(SequenceContext context);

        void SetSteps(std::vector<std::unique_ptr<ISequenceStep>> steps);

        // 전제조건(상태 머신 허용 여부) 확인 후 자동 운전 시작.
        rs::core::Status Start();

        void RequestPause();
        void RequestResume();
        void RequestStop();    // 정상 정지
        void RequestAbort();   // 즉시/안전 정지

        // 작업 스레드(설계서 4.2 Sequence Thread)에서 주기적으로 호출. 한 번에 한 단계분 진행.
        void Tick();

        bool IsRunning() const noexcept;
        bool IsPaused() const noexcept;

    private:
        void EnterStep(std::size_t index);
        void FailCurrent(std::wstring_view reason);
        void Finish(bool normal);

        SequenceContext                             m_context;
        std::vector<std::unique_ptr<ISequenceStep>> m_steps;
        std::size_t                                 m_current{ 0 };
        std::optional<TimeoutGuard>                 m_timeout;
        bool                                        m_entered{ false };

        std::atomic<bool> m_running{ false };
        std::atomic<bool> m_paused{ false };
        std::atomic<bool> m_stopRequested{ false };
        std::atomic<bool> m_abortRequested{ false };
    };
}
