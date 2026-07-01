#pragma once

#include "core/common/Result.h"
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

// 설계서 6.2 / 5.1.5: IAlarmService — 알람 발생, 해제, 활성 알람 조회, 호스트 보고 요청.
// 설계서 5.1.5.2(알람 생명주기) 반영: Detected -> Raised -> Acknowledged -> Cleared -> Archived.
namespace rs::domain
{
    enum class AlarmSeverity
    {
        Info,
        Warning,
        Error,
        Critical    // 인터락성 — 즉시 정지/금지 동작 유발
    };

    // 설계서 5.1.5.2: 알람 생명주기 상태.
    //   Detected      알람 조건 감지(디바운스/중복 확인) — Raise() 호출 시 내부적으로 즉시 Raised 로 넘어감.
    //   Raised        활성 알람으로 등록(UI 표시, 로그, 호스트 보고 요청).
    //   Acknowledged  사용자가 확인(확인자/시각 기록).
    //   Cleared       원인 조건 해소(Reset 가능 여부 판단, 호스트 해제 보고).
    //   Archived      이력 저장 완료(검색 가능한 알람 이력으로 보관).
    enum class AlarmLifecycle
    {
        Detected,
        Raised,
        Acknowledged,
        Cleared,
        Archived
    };

    constexpr std::wstring_view ToString(AlarmLifecycle s) noexcept
    {
        switch (s)
        {
        case AlarmLifecycle::Detected:     return L"Detected";
        case AlarmLifecycle::Raised:       return L"Raised";
        case AlarmLifecycle::Acknowledged: return L"Acknowledged";
        case AlarmLifecycle::Cleared:      return L"Cleared";
        case AlarmLifecycle::Archived:     return L"Archived";
        default:                           return L"Unknown";
        }
    }

    struct AlarmInfo
    {
        std::uint32_t code{ 0 };          // 설계서 5.1.5.1 알람 ID 체계(AlarmCode.h 참고)
        std::string   message;
        AlarmSeverity severity{ AlarmSeverity::Error };
        std::int64_t  timestamp{ 0 };     // Raised 시각(epoch ms)
        bool          active{ false };    // true = 아직 Archived 되지 않음(하위 호환 필드)

        AlarmLifecycle lifecycle{ AlarmLifecycle::Raised };
        std::string    acknowledgedBy;
        std::int64_t   acknowledgedAt{ 0 };
        std::int64_t   clearedAt{ 0 };
        std::int64_t   archivedAt{ 0 };
    };

    class IAlarmService
    {
    public:
        virtual ~IAlarmService() = default;

        // 알람 발생(Detected -> Raised). 이미 활성(미해소)인 동일 코드가 다시 발생하면
        // 중복으로 간주해 메시지만 갱신한다(5.1.5.2 "디바운스, 중복 여부 확인").
        virtual rs::core::Status Raise(std::uint32_t code, std::string const& message,
                                       AlarmSeverity severity) = 0;

        // 사용자 확인(Raised -> Acknowledged). ackBy = 확인한 사용자/계정 식별자.
        virtual rs::core::Status Acknowledge(std::uint32_t code, std::string const& ackBy) = 0;

        // 원인 조건 해소(-> Cleared). 아직 이력으로 넘어가지 않으며 Archive() 전까지 조회 가능.
        virtual rs::core::Status Clear(std::uint32_t code) = 0;

        // 이력 보관(Cleared -> Archived). Cleared 상태가 아니면 거부한다(원인 미해소 알람은 보관 불가).
        virtual rs::core::Status Archive(std::uint32_t code) = 0;

        // 아직 Archived 되지 않은 모든 알람(Raised/Acknowledged/Cleared 포함).
        virtual rs::core::Result<std::vector<AlarmInfo>> GetActive() = 0;

        // 원인이 아직 해소되지 않은(Raised/Acknowledged) 알람만 — "실제 활성 문제" 판단용.
        virtual rs::core::Result<std::vector<AlarmInfo>> GetUnresolved() = 0;

        // Archived 로 보관된 이력(검색 가능한 알람 이력, 5.1.5.2 Archived).
        virtual rs::core::Result<std::vector<AlarmInfo>> GetHistory() = 0;

        // 설계서 5.1.5.6: Reset 은 원인 해소 확인 없이 허용되어서는 안 된다.
        // 미해소(Raised/Acknowledged) 알람이 남아 있으면 false.
        virtual bool CanReset() const = 0;

        // 5.1.5.6 조건을 확인한 뒤에만 상태머신에 Reset 을 요청한다(단일 진입점 우회 금지).
        virtual rs::core::Status RequestReset(std::wstring_view requester) = 0;

        // 호스트(SECS/GEM)로 알람 보고 요청.
        virtual rs::core::Status ReportToHost(std::uint32_t code) = 0;
    };
}
