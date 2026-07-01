#pragma once

#include "core/domain/IAlarmService.h"
#include <map>
#include <mutex>
#include <vector>

// 설계서 6.2 / 5.1.5: IAlarmService 구체 구현.
// 알람 발생/해제 시 EventBus 로 상태를 발행하여 UI 등 구독자가 반응하게 한다.
// 알람 생명주기(5.1.5.2)를 관리: m_active 는 Archived 이전 모든 알람(Raised/Acknowledged/
// Cleared), m_history 는 Archived 로 보관된 이력을 담는다.
namespace rs::domain
{
    // 알람 상태 변경 이벤트 토픽. payload = 미해소(Raised/Acknowledged) 알람 중 대표 메시지
    // (없으면 빈 문자열 — 즉, 원인이 해소되면 Cleared 로 넘어가 바 색상은 초록으로 돌아간다).
    inline constexpr char AlarmChangedTopic[] = "alarm.changed";

    class AlarmService : public IAlarmService
    {
    public:
        static AlarmService& Instance() noexcept;

        rs::core::Status Raise(std::uint32_t code, std::string const& message, AlarmSeverity severity) override;
        rs::core::Status Acknowledge(std::uint32_t code, std::string const& ackBy) override;
        rs::core::Status Clear(std::uint32_t code) override;
        rs::core::Status Archive(std::uint32_t code) override;
        rs::core::Result<std::vector<AlarmInfo>> GetActive() override;
        rs::core::Result<std::vector<AlarmInfo>> GetUnresolved() override;
        rs::core::Result<std::vector<AlarmInfo>> GetHistory() override;
        bool CanReset() const override;
        rs::core::Status RequestReset(std::wstring_view requester) override;
        rs::core::Status ReportToHost(std::uint32_t code) override;

        AlarmService(AlarmService const&) = delete;
        AlarmService& operator=(AlarmService const&) = delete;

    private:
        AlarmService() = default;

        // 미해소(Raised/Acknowledged) 알람 중 대표 메시지. 없으면 빈 문자열.
        std::string ActiveMessage() const;   // m_mutex 보유 상태에서 호출

        mutable std::mutex                 m_mutex;
        std::map<std::uint32_t, AlarmInfo> m_active;    // Archived 이전(Raised/Acknowledged/Cleared)
        std::vector<AlarmInfo>             m_history;   // Archived 로 보관된 이력
    };
}
