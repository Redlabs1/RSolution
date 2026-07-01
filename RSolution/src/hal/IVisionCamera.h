#pragma once

#include "core/common/Result.h"
#include <cstdint>
#include <cstddef>
#include <span>
#include <memory>

// 설계서 6.2: IVisionCamera — 카메라 연결, 트리거, 이미지 취득, 노출/게인 설정, 상태 조회.
// 설계서 6.4.5.3: 이미지 버퍼는 span 으로 전달하되 비동기 수명이 필요하면 소유 객체/버퍼풀로 복사.
namespace rs::hal
{
    enum class CameraState
    {
        Disconnected,
        Connected,
        Acquiring,
        Error
    };

    // 취득된 한 프레임. 데이터는 소유(버퍼풀에서 대여한 메모리 등)한다.
    struct ImageFrame
    {
        std::uint32_t width{ 0 };
        std::uint32_t height{ 0 };
        std::uint32_t stride{ 0 };
        std::shared_ptr<std::byte[]> data;   // 소유 — 비동기 큐 전달 안전
        std::size_t   size{ 0 };
    };

    class IVisionCamera
    {
    public:
        virtual ~IVisionCamera() = default;

        virtual rs::core::Status Connect() = 0;
        virtual rs::core::Status Disconnect() = 0;
        virtual rs::core::Status Trigger() = 0;
        virtual rs::core::Result<ImageFrame> Acquire() = 0;
        virtual rs::core::Status SetExposure(double microseconds) = 0;
        virtual rs::core::Status SetGain(double gain) = 0;
        virtual rs::core::Result<CameraState> GetStatus() = 0;
    };
}
