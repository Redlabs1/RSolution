#pragma once

#include "hal/IMotionController.h"
#include <map>
#include <mutex>
#include <chrono>

// 설계서 3.2(검증성) / 9(2단계 공통 프레임워크): 하드웨어 없이 시퀀스/상태 전이를 검증하기 위한
// 가상 모션 컨트롤러. 이동 명령을 받으면 즉시 위치를 갱신하되, 지정된 지속시간 동안
// Moving 상태를 유지한 뒤 Idle 로 돌아가 실제 모션의 시간 특성을 근사한다.
namespace rs::simulator
{
    class SimulatedMotionController : public rs::hal::IMotionController
    {
    public:
        // moveDuration: MoveAbsolute/MoveRelative 호출 후 Moving 상태를 유지할 시간(간이 모델).
        explicit SimulatedMotionController(std::chrono::milliseconds moveDuration = std::chrono::milliseconds(300))
            : m_moveDuration(moveDuration) {}

        rs::core::Status Initialize(rs::hal::AxisId axis) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto& axisState = m_axes[axis];
            axisState.state = rs::hal::MotionState::Idle;
            return rs::core::Status::Success();
        }

        rs::core::Status EnableServo(rs::hal::AxisId axis, bool on) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_axes[axis].servoOn = on;
            return rs::core::Status::Success();
        }

        rs::core::Status Home(rs::hal::AxisId axis) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto& a = m_axes[axis];
            a.position = 0.0;
            a.homed = true;
            a.state = rs::hal::MotionState::Idle;
            return rs::core::Status::Success();
        }

        rs::core::Status MoveAbsolute(rs::hal::AxisId axis, double position, double /*velocity*/) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto& a = m_axes[axis];
            if (!a.servoOn)
                return rs::core::Error{ 1, "servo off" };
            a.position = position;
            a.state = rs::hal::MotionState::Moving;
            a.moveUntil = std::chrono::steady_clock::now() + m_moveDuration;
            return rs::core::Status::Success();
        }

        rs::core::Status MoveRelative(rs::hal::AxisId axis, double distance, double velocity) override
        {
            double base;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                base = m_axes[axis].position;
            }
            return MoveAbsolute(axis, base + distance, velocity);
        }

        rs::core::Status Stop(rs::hal::AxisId axis) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_axes[axis].state = rs::hal::MotionState::Idle;
            return rs::core::Status::Success();
        }

        rs::core::Result<rs::hal::AxisStatus> GetStatus(rs::hal::AxisId axis) override
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto& a = m_axes[axis];
            if (a.state == rs::hal::MotionState::Moving && std::chrono::steady_clock::now() >= a.moveUntil)
                a.state = rs::hal::MotionState::Idle;   // 이동 완료 시뮬레이션

            rs::hal::AxisStatus s;
            s.state = a.state;
            s.position = a.position;
            s.servoOn = a.servoOn;
            s.homed = a.homed;
            return s;
        }

    private:
        struct AxisState
        {
            rs::hal::MotionState state{ rs::hal::MotionState::Disabled };
            double position{ 0.0 };
            bool servoOn{ false };
            bool homed{ false };
            std::chrono::steady_clock::time_point moveUntil{};
        };

        std::mutex m_mutex;
        std::map<rs::hal::AxisId, AxisState> m_axes;
        std::chrono::milliseconds m_moveDuration;
    };
}
