#include "pch.h"
#include "core/domain/AlarmService.h"
#include "core/common/EventBus.h"
#include "core/domain/EquipmentStateMachine.h"
#include "core/infrastructure/logging/LogManager.h"
#include <chrono>

namespace rs::domain
{
    namespace
    {
        std::int64_t NowMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }

        void PublishAlarmChanged(std::string const& message)
        {
            rs::core::Event e;
            e.topic = AlarmChangedTopic;
            e.payload = message;          // 빈 문자열이면 미해소 알람 없음
            e.timestamp = NowMs();
            rs::core::EventBus::Instance().Publish(e);
        }

        // 설계서 7.1.7: 알람 등급에 따라 상태 전이를 유발할지 결정한다.
        // Info/Warning 은 상태를 유지하고, Major(Error)는 운전 중이면 정상 정지,
        // Fatal(Critical)은 즉시 안전 정지(Alarm) 로 전이한다. 전이는 상태머신의
        // 단일 진입점(RequestTransition, 7.1.5)만 사용한다.
        void ApplyAlarmStatePolicy(AlarmSeverity severity)
        {
            auto& sm = EquipmentStateMachine::Instance();
            switch (severity)
            {
            case AlarmSeverity::Info:
            case AlarmSeverity::Warning:
                // 상태 유지 — 전이 없음(로그/화면 표시만).
                break;

            case AlarmSeverity::Error:   // Major
            {
                const EquipmentState s = sm.State();
                if (s == EquipmentState::AutoRunning || s == EquipmentState::Paused)
                {
                    sm.RequestTransition(EquipmentCommand::Stop, L"Alarm",
                        L"Major 알람 - 공정 품질/장비 손상 우려로 정상 정지");
                }
                break;
            }

            case AlarmSeverity::Critical:   // Fatal
                sm.RequestTransition(EquipmentCommand::AlarmRaised, L"Alarm",
                    L"Fatal 알람 - 즉시 안전 정지");
                break;
            }
        }
    }

    AlarmService& AlarmService::Instance() noexcept
    {
        static AlarmService instance;
        return instance;
    }

    std::string AlarmService::ActiveMessage() const
    {
        // 뒤에서부터(코드값 큰 순) 훑어 미해소(Raised/Acknowledged) 상태인 첫 항목을 대표로 사용.
        // Cleared 는 원인이 해소된 것이므로 대표 메시지(=바 색상 트리거)에서 제외한다.
        for (auto it = m_active.rbegin(); it != m_active.rend(); ++it)
        {
            auto const& info = it->second;
            if (info.lifecycle == AlarmLifecycle::Raised || info.lifecycle == AlarmLifecycle::Acknowledged)
                return info.message;
        }
        return {};
    }

    rs::core::Status AlarmService::Raise(std::uint32_t code, std::string const& message, AlarmSeverity severity)
    {
        std::string current;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_active.find(code);
            if (it != m_active.end() && it->second.lifecycle != AlarmLifecycle::Cleared)
            {
                // 이미 미해소 상태로 활성 중 — 중복 발생(5.1.5.2 디바운스/중복 확인). 내용만 갱신.
                it->second.message = message;
                it->second.severity = severity;
            }
            else
            {
                // 신규 발생이거나, 이전에 Cleared 되었다가 재발생(재활성화).
                AlarmInfo info;
                info.code = code;
                info.message = message;
                info.severity = severity;
                info.timestamp = NowMs();
                info.active = true;
                info.lifecycle = AlarmLifecycle::Raised;
                m_active[code] = info;
            }
            current = ActiveMessage();
        }
        PublishAlarmChanged(current);   // 잠금 밖에서 발행
        ApplyAlarmStatePolicy(severity);   // 7.1.7: 등급별 상태 전이
        RS_WARN(rs::LogChannel::Exception, L"Alarm raised: code=" << code);
        return rs::core::Status::Success();
    }

    rs::core::Status AlarmService::Acknowledge(std::uint32_t code, std::string const& ackBy)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_active.find(code);
            if (it == m_active.end())
                return rs::core::Error{ 30, "alarm not found (acknowledge): " + std::to_string(code) };
            if (it->second.lifecycle == AlarmLifecycle::Archived)
                return rs::core::Error{ 31, "alarm already archived: " + std::to_string(code) };
            if (it->second.lifecycle == AlarmLifecycle::Raised)
            {
                it->second.lifecycle = AlarmLifecycle::Acknowledged;
                it->second.acknowledgedBy = ackBy;
                it->second.acknowledgedAt = NowMs();
            }
            // 이미 Acknowledged/Cleared 면 멱등 처리(성공으로 간주).
        }
        RS_INFO(rs::LogChannel::Exception, L"Alarm acknowledged: code=" << code);
        return rs::core::Status::Success();
    }

    rs::core::Status AlarmService::Clear(std::uint32_t code)
    {
        std::string current;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_active.find(code);
            if (it != m_active.end() && it->second.lifecycle != AlarmLifecycle::Archived)
            {
                it->second.lifecycle = AlarmLifecycle::Cleared;
                it->second.clearedAt = NowMs();
            }
            current = ActiveMessage();
        }
        PublishAlarmChanged(current);
        RS_INFO(rs::LogChannel::Exception, L"Alarm cleared: code=" << code);
        return rs::core::Status::Success();
    }

    rs::core::Status AlarmService::Archive(std::uint32_t code)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_active.find(code);
        if (it == m_active.end())
            return rs::core::Error{ 32, "alarm not found (archive): " + std::to_string(code) };
        if (it->second.lifecycle != AlarmLifecycle::Cleared)
            return rs::core::Error{ 33, "alarm must be cleared before archiving: " + std::to_string(code) };

        AlarmInfo archived = it->second;
        archived.lifecycle = AlarmLifecycle::Archived;
        archived.archivedAt = NowMs();
        archived.active = false;
        m_history.push_back(archived);
        m_active.erase(it);

        RS_INFO(rs::LogChannel::Exception, L"Alarm archived: code=" << code);
        return rs::core::Status::Success();
    }

    rs::core::Result<std::vector<AlarmInfo>> AlarmService::GetActive()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<AlarmInfo> out;
        out.reserve(m_active.size());
        for (auto const& [code, info] : m_active)
            out.push_back(info);
        return out;
    }

    rs::core::Result<std::vector<AlarmInfo>> AlarmService::GetUnresolved()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<AlarmInfo> out;
        for (auto const& [code, info] : m_active)
            if (info.lifecycle == AlarmLifecycle::Raised || info.lifecycle == AlarmLifecycle::Acknowledged)
                out.push_back(info);
        return out;
    }

    rs::core::Result<std::vector<AlarmInfo>> AlarmService::GetHistory()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_history;
    }

    bool AlarmService::CanReset() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto const& [code, info] : m_active)
            if (info.lifecycle == AlarmLifecycle::Raised || info.lifecycle == AlarmLifecycle::Acknowledged)
                return false;   // 5.1.5.6: 원인 미해소 알람이 남아있으면 Reset 불가
        return true;
    }

    rs::core::Status AlarmService::RequestReset(std::wstring_view requester)
    {
        if (!CanReset())
            return rs::core::Error{ 40, "reset rejected: unresolved alarms remain (5.1.5.6)" };

        auto& sm = EquipmentStateMachine::Instance();
        auto r = sm.RequestTransition(EquipmentCommand::Reset, requester,
            L"알람 원인 해소 확인 후 리셋(5.1.5.6)");
        if (!r.allowed)
            return rs::core::Error{ 41, "state machine rejected reset" };

        // 리셋 성공 — 원인 해소(Cleared) 상태인 알람들을 이력으로 보관한다.
        std::vector<std::uint32_t> toArchive;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto const& [code, info] : m_active)
                if (info.lifecycle == AlarmLifecycle::Cleared)
                    toArchive.push_back(code);
        }
        for (auto code : toArchive)
            Archive(code);

        return rs::core::Status::Success();
    }

    rs::core::Status AlarmService::ReportToHost(std::uint32_t code)
    {
        RS_INFO(rs::LogChannel::Event, L"ReportToHost alarm code=" << code);
        return rs::core::Status::Success();
    }
}
