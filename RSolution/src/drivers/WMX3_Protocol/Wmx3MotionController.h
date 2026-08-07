#pragma once

// SoftServo WMX3 모션 어댑터 — rs::hal::IMotionController 구현 (설계서 6.2 drivers/ 계층).
//   - 축별 운전 파라미터는 전역 조회가 아니라 Wmx3AxisParam 주입으로 받는다
//   - 반환은 모듈 경계 규약대로 rs::core::Result / Status (설계서 6.3, 예외 없음)
// 헤더는 WMX3 SDK 비의존(설계서 6.4.4 Adapter 격리) — SDK 타입은 .cpp 의 Impl 에만 존재.
// WMX3 SDK 미설치 PC 에서도 빌드된다: .cpp 에서 __has_include(<WMX3Api.h>) 로 감지,
// 미설치 시 모든 호출이 wmx3_errc::SdkNotInstalled 에러를 반환한다(SDK 설치 시 자동 활성화).
// 스레드 안전: 모든 SDK 호출을 뮤텍스로 직렬화.
//
// 단위 규약: 위치 mm, 속도 mm/s, 가감속은 시간(ms) — SDK 내부 단위(µm)로는 unitScale 로 변환.

#include <cstdint>
#include <memory>
#include <string>

#include "core/common/Result.h"
#include "hal/IMotionController.h"

namespace rs::drivers
{
    // 에러 코드 (rs::core::Error::code). 음수는 어댑터 자체 오류, 양수는 WMX3 API 반환 코드 그대로.
    namespace wmx3_errc
    {
        constexpr int SdkNotInstalled = -1;  // WMX3 SDK 가 설치되지 않음
        constexpr int NotOpen         = -2;  // Open() 이전에 호출됨
        constexpr int Timeout         = -3;  // WaitMoveDone 시간 초과
        constexpr int ServoAlarm      = -4;  // 이동 중 앰프 알람 감지
        constexpr int ServoOffline    = -5;  // 이동 중 서보 오프라인 감지
        constexpr int LimitHit        = -6;  // 이동 중 리밋 센서 감지
        constexpr int EngineNotReady  = -7;  // 통신 엔진이 Communicating 상태로 진입 실패
    }

    // 축별 운전 파라미터. Initialize() 전에 SetAxisParam 으로 주입한다.
    struct Wmx3AxisParam
    {
        double autoVelocity{ 10.0 };     // 자동 운전 속도 (mm/s)
        double manualVelocity{ 5.0 };    // 수동 운전 속도 (mm/s)
        double homeVelocity{ 5.0 };      // 원점 복귀 속도 (mm/s)
        double jogHighVelocity{ 10.0 };  // 조그 고속 (mm/s)
        double jogLowVelocity{ 1.0 };    // 조그 저속 (mm/s)
        double accTimeMs{ 100.0 };       // 가속 시간 (ms) — 가속도 = 속도 / (accTimeMs/1000)
        double decTimeMs{ 100.0 };       // 감속 시간 (ms)
        double inPosRange{ 0.003 };      // 인포지션 허용 범위 (mm)
        double unitScale{ 1000.0 };      // 사용자 단위(mm) → SDK 단위(µm) 배율
    };

    // 확장 상태 조회. hal::AxisStatus 로는 표현되지 않는 센서/토크/상태 문자열까지 한 번에 반환한다.
    struct Wmx3AxisDetail
    {
        bool servoOn{ false };
        bool servoAlarm{ false };
        bool homeDone{ false };
        bool inPosition{ false };
        bool positiveLimit{ false };
        bool negativeLimit{ false };
        bool homeSensor{ false };
        double commandPosition{ 0.0 };  // mm
        double actualPosition{ 0.0 };   // mm
        double actualTorque{ 0.0 };
        std::wstring opState;           // 예: L"Idle", L"Pos", L"Home"
        std::wstring homeState;         // 예: L"Idle", L"Complete", L"HSSearch"
    };

    // EtherCAT 슬레이브 건강 상태.
    struct Wmx3SlaveHealth
    {
        bool allOperational{ false };  // 전 슬레이브 Op 상태
        bool anyLost{ false };         // None/Init 로 이탈한 슬레이브 존재
        int  onlineCount{ 0 };
    };

    class Wmx3MotionController final : public rs::hal::IMotionController
    {
    public:
        Wmx3MotionController();
        ~Wmx3MotionController() override;

        Wmx3MotionController(Wmx3MotionController const&) = delete;
        Wmx3MotionController& operator=(Wmx3MotionController const&) = delete;

        // 앱 전역 공유 인스턴스(페이지 전환과 무관하게 연결 유지).
        static Wmx3MotionController& Instance();

        // 빌드에 WMX3 SDK 가 포함되었는지(설치 후 리빌드하면 true).
        static bool IsSdkAvailable() noexcept;

        // ---- 엔진 수명주기 ----
        // CreateDevice + StartCommunication(Communicating 진입까지 재시도).
        // paramXmlPath 를 주면 config->ImportAndSetAll 로 축 파라미터 XML 일괄 적용.
        core::Status Open(std::string const& enginePath = DefaultEnginePath(),
                          std::string const& paramXmlPath = {});
        void         Close();
        bool         IsOpen() const noexcept;

        static std::string DefaultEnginePath() { return "C:\\Program Files\\SoftServo\\WMX3\\"; }

        // ---- 축별 파라미터 ----
        // Initialize(axis) 전에 설정하면 홈 속도/인포지션 범위가 SDK 에 반영된다.
        core::Status                 SetAxisParam(hal::AxisId axis, Wmx3AxisParam const& param);
        core::Result<Wmx3AxisParam>  GetAxisParam(hal::AxisId axis) const;

        // ---- IMotionController (설계서 6.2) ----
        core::Status Initialize(hal::AxisId axis) override;                 // 홈 파라미터/인포지션 범위 적용
        core::Status EnableServo(hal::AxisId axis, bool on) override;
        core::Status Home(hal::AxisId axis) override;                       // home->StartHome
        core::Status MoveAbsolute(hal::AxisId axis, double position, double velocity) override;
        core::Status MoveRelative(hal::AxisId axis, double distance, double velocity) override;
        core::Status Stop(hal::AxisId axis) override;
        core::Result<hal::AxisStatus> GetStatus(hal::AxisId axis) override;

        // ---- 확장 (IMotionController 범위 밖) ----
        // 축 기본값 대신 가감속 시간을 호출 시점에 지정하는 이동.
        core::Status MoveAbsoluteEx(hal::AxisId axis, double position, double velocity,
                                    double accTimeMs, double decTimeMs);
        core::Status MoveRelativeEx(hal::AxisId axis, double distance, double velocity,
                                    double accTimeMs, double decTimeMs);

        core::Status StartJog(hal::AxisId axis, bool forward, bool fast);   // 조그 시작 (JogCommand)
        core::Status StopJog(hal::AxisId axis);
        core::Status JogStep(hal::AxisId axis, double step, bool fast);     // 현재 위치 + step 피치 이동

        core::Status AlarmReset(hal::AxisId axis);                          // ClearAxisAlarm → ClearAmpAlarm
        core::Status ClearHomeDone(hal::AxisId axis);                       // home->SetHomeDone(axis, 0)
        core::Status EmergencyStop();                                       // ExecEStop(Final)
        core::Status ReleaseEmergencyStop();
        core::Status StopAll();                                             // 전 축 정지 (AllMotorExecQuickStop)

        // 이동 완료 대기. 완료 전 알람/오프라인/리밋 감지 시 전 축 정지 후 해당 에러 반환.
        core::Status WaitMoveDone(hal::AxisId axis, int timeoutMs);

        core::Result<Wmx3AxisDetail> GetDetail(hal::AxisId axis);

        // 갠트리 등 마스터-슬레이브 동기 (sync API).
        core::Status SetSync(hal::AxisId master, hal::AxisId slave);
        core::Status ResolveSync(hal::AxisId slave);

        core::Result<Wmx3SlaveHealth> CheckSlaves();                        // EtherCAT 마스터 정보 조회

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
