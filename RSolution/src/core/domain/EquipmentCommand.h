#pragma once

#include <string_view>

// 설계서 7.1.6: 상태별 허용 명령. 사용자/호스트 명령과 내부 이벤트(전이 트리거)를 함께 정의한다.
namespace rs::domain
{
    enum class EquipmentCommand
    {
        // --- 사용자 / 호스트 명령 ---
        Start,            // Idle → AutoRunning
        Pause,            // AutoRunning → Paused
        Resume,           // Paused → AutoRunning
        Stop,             // 정상 정지 → Stopping
        Abort,            // 즉시/안전 정지 (Stop 과 구분, 7.1.8)
        Reset,            // Alarm 해제
        Manual,           // Idle 에서 수동 모드(상태 유지)
        RecipeSelect,     // 레시피 선택(상태 유지)
        Maintenance,      // Idle → Maintenance
        ExitMaintenance,  // Maintenance → Idle
        CancelInit,       // Initializing 취소
        Diagnostics,      // 진단(상태 유지)
        ManualMove,       // Maintenance 수동 축 이동(상태 유지)
        IoTest,           // Maintenance I/O 테스트(상태 유지)

        // --- 내부 이벤트(시퀀스/초기화/알람이 발생시키는 전이) ---
        PowerOn,          // PowerOff → Initializing
        InitComplete,     // Initializing → Idle
        InitFailed,       // Initializing → Alarm
        ProcessComplete,  // AutoRunning → Stopping (정상 완료)
        StopComplete,     // Stopping → Idle
        AlarmRaised       // 운전 중 Major/Fatal 알람 → Alarm
    };

    std::wstring_view ToString(EquipmentCommand cmd) noexcept;
}
