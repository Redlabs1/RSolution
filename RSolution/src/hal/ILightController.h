#pragma once

#include "core/common/Result.h"
#include <cstdint>

// 설계서 v2 5.5.2: ILightController — 조명 채널 선택, 밝기 설정, 스트로브 제어, 조명 상태 조회.
// HAL 계층: 조명 컨트롤러 벤더 SDK 는 구현체(어댑터) 내부에 격리한다(설계서 5.5.6).
namespace rs::hal
{
    using LightChannel = std::uint32_t;

    enum class LightState
    {
        Off,
        On,
        Strobe,
        Error
    };

    class ILightController
    {
    public:
        virtual ~ILightController() = default;

        virtual rs::core::Status Connect() = 0;
        virtual rs::core::Status Disconnect() = 0;
        // 밝기: 0(소등) ~ 100(최대). 채널별 독립 제어.
        virtual rs::core::Status SetBrightness(LightChannel channel, std::uint32_t percent) = 0;
        virtual rs::core::Status TurnOn(LightChannel channel) = 0;
        virtual rs::core::Status TurnOff(LightChannel channel) = 0;
        // 스트로브: 트리거 신호와 동기한 펄스 점등(마이크로초 단위 폭).
        virtual rs::core::Status SetStrobe(LightChannel channel, std::uint32_t pulseWidthUs) = 0;
        virtual rs::core::Result<LightState> GetStatus(LightChannel channel) = 0;
    };
}
