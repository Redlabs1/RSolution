#pragma once

#include "sequence/ISequenceStep.h"
#include "hal/IMotionController.h"
#include <string>

// IMotionController 를 사용해 지정 축을 목표 위치로 이동시키는 단계.
// 시작 조건: 컨텍스트에 모션 컨트롤러가 있어야 함. 완료 조건: 축 상태가 Idle(정지)로 복귀.
// 위험한 이동 중 일시정지는 허용하지 않는다(설계서 7.1.8 Step별 Pause 허용 여부).
namespace rs::sequence::steps
{
    class MoveAxisStep : public ISequenceStep
    {
    public:
        MoveAxisStep(std::wstring name, rs::hal::AxisId axis, double position, double velocity)
            : m_name(std::move(name)), m_axis(axis), m_position(position), m_velocity(velocity) {}

        std::wstring_view Name() const noexcept override { return m_name; }

        bool CanStart(SequenceContext& ctx) override
        {
            return ctx.motion != nullptr;
        }

        void OnEnter(SequenceContext& ctx) override
        {
            m_moveRequested = false;
            m_failed = false;
            if (ctx.motion)
            {
                auto r = ctx.motion->MoveAbsolute(m_axis, m_position, m_velocity);
                m_moveRequested = r.ok();
                m_failed = !r.ok();
            }
        }

        StepResult Execute(SequenceContext& ctx) override
        {
            if (m_failed || !ctx.motion)
                return StepResult::Failed;

            auto status = ctx.motion->GetStatus(m_axis);
            if (!status.ok())
                return StepResult::Failed;

            using rs::hal::MotionState;
            switch (status.value().state)
            {
            case MotionState::Idle:   return StepResult::Completed;
            case MotionState::Error:  return StepResult::Failed;
            default:                  return StepResult::Running;   // Moving/Homing/Disabled 등
            }
        }

        void OnExit(SequenceContext&) override {}

        void OnFailure(SequenceContext& ctx) override
        {
            if (ctx.motion)
                ctx.motion->Stop(m_axis);   // 안전 정지
        }

        std::chrono::milliseconds Timeout() const noexcept override { return std::chrono::seconds(30); }
        bool CanPause() const noexcept override { return false; }   // 이동 중 일시정지 금지

    private:
        std::wstring       m_name;
        rs::hal::AxisId    m_axis;
        double             m_position;
        double             m_velocity;
        bool               m_moveRequested{ false };
        bool               m_failed{ false };
    };
}
