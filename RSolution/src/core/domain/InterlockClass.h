#pragma once

#include <string_view>

// 설계서 5.1.5.3: 인터락 분류. 목적/위험도에 따라 분류해 유지보수·수동·자동·원격 운전별
// 허용 조건을 구분한다. 5.1.5.5(평가 주기)의 처리 위치와도 대응된다.
namespace rs::domain
{
    enum class InterlockClass
    {
        Safety,             // 작업자 안전 보호 — EMO, Door Open, Safety PLC Trip (Real-time Loop, 최상 우선순위)
        EquipmentProtection,// 장비 파손 방지 — Over Travel, Servo Alarm, Vacuum Fail (10~100ms, 높음)
        ProcessProtection,  // 공정 품질 보호 — Temperature Out of Range, Pressure Low (10~100ms, 높음)
        Operation           // 운전 조건 제한 — Recipe Not Selected, Axis Not Homed, Lot Not Ready (명령 실행 전, 중간)
    };

    constexpr std::wstring_view ToString(InterlockClass c) noexcept
    {
        switch (c)
        {
        case InterlockClass::Safety:              return L"Safety";
        case InterlockClass::EquipmentProtection:  return L"EquipmentProtection";
        case InterlockClass::ProcessProtection:    return L"ProcessProtection";
        case InterlockClass::Operation:            return L"Operation";
        default:                                   return L"Unknown";
        }
    }

    // 자동 운전 시작을 즉시 차단해야 하는 분류인지(설계서 7.1.6 "인터락 조건 미만족 시 Start 제한").
    // Safety/EquipmentProtection/ProcessProtection 은 즉시 차단, Operation 은 명령 실행 전 검증 대상.
    constexpr bool BlocksAutoStart(InterlockClass c) noexcept
    {
        return c == InterlockClass::Safety
            || c == InterlockClass::EquipmentProtection
            || c == InterlockClass::ProcessProtection
            || c == InterlockClass::Operation;
    }
}
