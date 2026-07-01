#pragma once

#include "sequence/ISequenceStep.h"
#include <string>

// 하드웨어 없이 시퀀스 골격을 검증하기 위한 최소 단계: 지정 시간만큼 대기 후 완료.
// 시작 조건은 항상 참, 타임아웃은 대기시간의 3배 여유를 둔다.
namespace rs::sequence::steps
{
    class DelayStep : public ISequenceStep
    {
    public:
        DelayStep(std::wstring name, std::chrono::milliseconds delay, bool canPause = true)
            : m_name(std::move(name)), m_delay(delay), m_canPause(canPause) {}

        std::wstring_view Name() const noexcept override { return m_name; }

        bool CanStart(SequenceContext&) override { return true; }

        void OnEnter(SequenceContext&) override
        {
            m_deadline = std::chrono::steady_clock::now() + m_delay;
        }

        StepResult Execute(SequenceContext&) override
        {
            return (std::chrono::steady_clock::now() >= m_deadline) ? StepResult::Completed : StepResult::Running;
        }

        void OnExit(SequenceContext&) override {}
        void OnFailure(SequenceContext&) override {}

        std::chrono::milliseconds Timeout() const noexcept override { return m_delay * 3 + std::chrono::milliseconds(1000); }
        bool CanPause() const noexcept override { return m_canPause; }

    private:
        std::wstring m_name;
        std::chrono::milliseconds m_delay;
        bool m_canPause;
        std::chrono::steady_clock::time_point m_deadline;
    };
}
