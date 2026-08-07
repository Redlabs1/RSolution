#pragma once

// SoftServo WMX3 I/O 어댑터 — rs::hal::IIoProvider 구현 (설계서 6.2 drivers/ 계층).
//   - 비트 주소를 port(=addr/8) + bit(=addr%8) 로 분해하는 규약 유지
//   - BOOL/전역 대신 rs::core::Result / Status 반환 (설계서 6.3)
//   - WMX3 에 I/O 이벤트 콜백이 없어 IIoProvider::Subscribe 는 폴링 워커 스레드로 구현
// 헤더는 WMX3 SDK 비의존(설계서 6.4.4) — SDK 타입은 .cpp 의 Impl 에만 존재.
// SDK 미설치 PC 에서도 빌드된다(모든 호출이 wmx3_errc::SdkNotInstalled 반환).
//
// 주의: WMX3 엔진은 모션(Wmx3MotionController)과 I/O 가 각각 자기 디바이스 핸들을 연다.

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "core/common/Result.h"
#include "drivers/WMX3_Protocol/Wmx3MotionController.h"   // wmx3_errc 공용
#include "hal/IIoProvider.h"

namespace rs::drivers
{
    class Wmx3IoProvider final : public rs::hal::IIoProvider
    {
    public:
        Wmx3IoProvider();
        ~Wmx3IoProvider() override;

        Wmx3IoProvider(Wmx3IoProvider const&) = delete;
        Wmx3IoProvider& operator=(Wmx3IoProvider const&) = delete;

        // 앱 전역 공유 인스턴스(페이지 전환과 무관하게 연결 유지).
        static Wmx3IoProvider& Instance();

        // ---- 엔진 수명주기 ----
        core::Status Open(std::string const& enginePath = Wmx3MotionController::DefaultEnginePath());
        void         Close();
        bool         IsOpen() const noexcept;

        // ---- IIoProvider (설계서 6.2) ----
        // point 는 비트 단위 통짜 주소 — 내부에서 port = point/8, bit = point%8 로 분해한다.
        core::Result<bool>   ReadDigital(hal::IoPoint point) override;
        core::Status         WriteDigital(hal::IoPoint point, bool value) override;
        // 아날로그는 채널 인덱스를 그대로 사용(GetInAnalogDataInt / SetOutAnalogDataInt).
        core::Result<double> ReadAnalog(hal::IoPoint point) override;
        core::Status         WriteAnalog(hal::IoPoint point, double value) override;

        // 입력 변화 구독. WMX3 는 I/O 이벤트 콜백이 없어 폴링 워커가 변화를 감지해 통지한다.
        // 첫 구독 시 워커가 시작되고, 마지막 구독 해제 시 정지한다.
        hal::IoSubscription Subscribe(hal::IoPoint point, hal::IoEventHandler handler) override;
        void                Unsubscribe(hal::IoSubscription token) override;

        // 구독 폴링 주기(기본 20ms). 워커 동작 중에도 변경 가능.
        void SetPollInterval(std::chrono::milliseconds interval);

        // ---- 확장 — 출력 코일에 현재 실려 있는 값 조회(입력이 아니라 출력을 되읽는다) ----
        core::Result<bool> ReadDigitalOutput(hal::IoPoint point);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
