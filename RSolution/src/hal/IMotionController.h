#pragma once

#include "core/common/Result.h"
#include <cstdint>

// 설계서 6.2: IMotionController — 축 초기화, 서보 제어, 원점 복귀, 이동 명령, 정지, 상태 조회.
// HAL 계층: 벤더 SDK 의존성을 인터페이스 뒤로 숨긴다(설계서 4 / 6.4.4 Adapter 격리).
namespace rs::hal
{
    using AxisId = std::uint32_t;

    enum class MotionState
    {
        Disabled,
        Idle,
        Homing,
        Moving,
        Error
    };

    struct AxisStatus
    {
        MotionState state{ MotionState::Disabled };
        double      position{ 0.0 };
        bool        servoOn{ false };
        bool        homed{ false };
    };

    class IMotionController
    {
    public:
        virtual ~IMotionController() = default;

        virtual rs::core::Status Initialize(AxisId axis) = 0;
        virtual rs::core::Status EnableServo(AxisId axis, bool on) = 0;
        virtual rs::core::Status Home(AxisId axis) = 0;
        virtual rs::core::Status MoveAbsolute(AxisId axis, double position, double velocity) = 0;
        virtual rs::core::Status MoveRelative(AxisId axis, double distance, double velocity) = 0;
        virtual rs::core::Status Stop(AxisId axis) = 0;
        virtual rs::core::Result<AxisStatus> GetStatus(AxisId axis) = 0;
    };
}
