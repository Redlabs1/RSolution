#pragma once

#include <string_view>
#include <string>

// 설계서 7.1.3: 장비 상태 머신의 주요 상태.
// 단순 화면 표시가 아니라 명령 허용 여부, 안전 동작, 복구 절차, 호스트 보고 기준의 중심 모델이다.
namespace rs::domain
{
    enum class EquipmentState
    {
        PowerOff,      // 프로그램 미실행/전원 꺼짐. 시작 시 Initializing 으로.
        Initializing,  // 설정 로딩, 장치 연결, 모션/IO 초기화, 원점 복귀 준비.
        Idle,          // 운전 대기. 자동 시작/수동 조작/레시피 선택/유지보수 진입 가능.
        AutoRunning,   // 자동 운전 시퀀스 수행 중. 레시피 변경·위험 수동조작 제한.
        Paused,        // 일시정지. Resume/Stop/Abort 등 제한된 명령만 허용.
        Stopping,      // 정상 정지 절차. 안전 위치 이동/출력 안전값 복귀.
        Alarm,         // 복구 필요 알람/인터락. 자동 시작 금지, Reset 절차 필요.
        Maintenance    // 유지보수/점검. 작업 완료 후 Idle.
    };

    constexpr std::wstring_view ToString(EquipmentState s) noexcept
    {
        switch (s)
        {
        case EquipmentState::PowerOff:     return L"PowerOff";
        case EquipmentState::Initializing: return L"Initializing";
        case EquipmentState::Idle:         return L"Idle";
        case EquipmentState::AutoRunning:  return L"AutoRunning";
        case EquipmentState::Paused:       return L"Paused";
        case EquipmentState::Stopping:     return L"Stopping";
        case EquipmentState::Alarm:        return L"Alarm";
        case EquipmentState::Maintenance:  return L"Maintenance";
        default:                           return L"Unknown";
        }
    }

    // EventBus 페이로드 등 UTF-8 문자열이 필요한 곳에서 사용. 상태 이름은 모두 ASCII 이므로
    // 단순 문자 단위 변환으로 충분하다.
    inline std::string ToUtf8(EquipmentState s)
    {
        auto sv = ToString(s);
        std::string out;
        out.reserve(sv.size());
        for (wchar_t ch : sv)
            out.push_back(static_cast<char>(ch));
        return out;
    }
}
