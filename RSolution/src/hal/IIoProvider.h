#pragma once

#include "core/common/Result.h"
#include <functional>
#include <cstdint>

// 설계서 6.2: IIoProvider — 디지털/아날로그 입력 조회, 출력 제어, I/O 이벤트 구독.
namespace rs::hal
{
    using IoPoint = std::uint32_t;

    struct IoChangeEvent
    {
        IoPoint point{ 0 };
        bool    value{ false };
    };

    using IoEventHandler = std::function<void(IoChangeEvent const&)>;
    using IoSubscription = std::uint64_t;

    class IIoProvider
    {
    public:
        virtual ~IIoProvider() = default;

        virtual rs::core::Result<bool>   ReadDigital(IoPoint point) = 0;
        virtual rs::core::Result<double> ReadAnalog(IoPoint point) = 0;
        virtual rs::core::Status         WriteDigital(IoPoint point, bool value) = 0;
        virtual rs::core::Status         WriteAnalog(IoPoint point, double value) = 0;

        // 디지털 입력 변화 구독. 반환 토큰으로 Unsubscribe.
        virtual IoSubscription Subscribe(IoPoint point, IoEventHandler handler) = 0;
        virtual void           Unsubscribe(IoSubscription token) = 0;
    };
}
