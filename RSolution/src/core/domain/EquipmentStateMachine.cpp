#include "pch.h"
#include "core/domain/EquipmentStateMachine.h"
#include "core/common/EventBus.h"
#include <chrono>

namespace rs::domain
{
    EquipmentStateMachine& EquipmentStateMachine::Instance() noexcept
    {
        static EquipmentStateMachine instance;
        return instance;
    }

    std::wstring_view ToString(EquipmentCommand cmd) noexcept
    {
        switch (cmd)
        {
        case EquipmentCommand::Start:           return L"Start";
        case EquipmentCommand::Pause:           return L"Pause";
        case EquipmentCommand::Resume:          return L"Resume";
        case EquipmentCommand::Stop:            return L"Stop";
        case EquipmentCommand::Abort:           return L"Abort";
        case EquipmentCommand::Reset:           return L"Reset";
        case EquipmentCommand::Manual:          return L"Manual";
        case EquipmentCommand::RecipeSelect:    return L"RecipeSelect";
        case EquipmentCommand::Maintenance:     return L"Maintenance";
        case EquipmentCommand::ExitMaintenance: return L"ExitMaintenance";
        case EquipmentCommand::CancelInit:      return L"CancelInit";
        case EquipmentCommand::Diagnostics:     return L"Diagnostics";
        case EquipmentCommand::ManualMove:      return L"ManualMove";
        case EquipmentCommand::IoTest:          return L"IoTest";
        case EquipmentCommand::PowerOn:         return L"PowerOn";
        case EquipmentCommand::InitComplete:    return L"InitComplete";
        case EquipmentCommand::InitFailed:      return L"InitFailed";
        case EquipmentCommand::ProcessComplete: return L"ProcessComplete";
        case EquipmentCommand::StopComplete:    return L"StopComplete";
        case EquipmentCommand::AlarmRaised:     return L"AlarmRaised";
        default:                                return L"Unknown";
        }
    }

    // 전이 테이블(설계서 7.1.4 흐름 + 7.1.6 허용 명령). 허용되지 않으면 nullopt,
    // 허용되면 목표 상태(상태를 유지하는 허용 명령은 현재 상태와 동일 값).
    std::optional<EquipmentState> EquipmentStateMachine::NextState(EquipmentState s, EquipmentCommand c) noexcept
    {
        using S = EquipmentState;
        using C = EquipmentCommand;

        switch (s)
        {
        case S::PowerOff:
            if (c == C::PowerOn) return S::Initializing;
            return std::nullopt;   // 모든 제어 명령 금지

        case S::Initializing:
            switch (c)
            {
            case C::InitComplete: return S::Idle;
            case C::InitFailed:   return S::Alarm;
            case C::CancelInit:   return S::PowerOff;
            case C::AlarmRaised:  return S::Alarm;
            default:              return std::nullopt;   // 자동운전/레시피변경/수동이동 제한
            }

        case S::Idle:
            switch (c)
            {
            case C::Start:        return S::AutoRunning;
            case C::Maintenance:  return S::Maintenance;
            case C::Manual:       return S::Idle;          // 허용, 상태 유지
            case C::RecipeSelect: return S::Idle;          // 허용, 상태 유지
            case C::AlarmRaised:  return S::Alarm;
            default:              return std::nullopt;
            }

        case S::AutoRunning:
            switch (c)
            {
            case C::Pause:           return S::Paused;
            case C::Stop:            return S::Stopping;
            case C::Abort:           return S::Stopping;   // 즉시/안전 정지
            case C::ProcessComplete: return S::Stopping;   // 정상 완료
            case C::AlarmRaised:     return S::Alarm;
            default:                 return std::nullopt;   // 레시피변경/위험 수동조작/유지보수 제한
            }

        case S::Paused:
            switch (c)
            {
            case C::Resume:      return S::AutoRunning;
            case C::Stop:        return S::Stopping;
            case C::Abort:       return S::Stopping;
            case C::AlarmRaised: return S::Alarm;
            default:             return std::nullopt;
            }

        case S::Stopping:
            switch (c)
            {
            case C::Abort:        return S::Stopping;       // 허용, 정지 가속(상태 유지)
            case C::StopComplete: return S::Idle;
            case C::AlarmRaised:  return S::Alarm;
            default:              return std::nullopt;       // Start/Resume/RecipeSelect 금지
            }

        case S::Alarm:
            switch (c)
            {
            // 7.1.8: 해제 후 무조건 Idle 가 아니라 재초기화 필요 여부는 상위 정책에서 판단.
            case C::Reset:       return S::Idle;
            case C::Diagnostics: return S::Alarm;            // 허용, 상태 유지
            default:             return std::nullopt;        // Start/Resume/재개 금지
            }

        case S::Maintenance:
            switch (c)
            {
            case C::ExitMaintenance: return S::Idle;
            case C::ManualMove:      return S::Maintenance;  // 허용, 상태 유지
            case C::IoTest:          return S::Maintenance;  // 허용, 상태 유지
            case C::Diagnostics:     return S::Maintenance;  // 허용, 상태 유지
            case C::AlarmRaised:     return S::Alarm;
            default:                 return std::nullopt;     // 자동운전 시작/호스트 Start 금지
            }

        default:
            return std::nullopt;
        }
    }

    EquipmentState EquipmentStateMachine::State() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_state;
    }

    bool EquipmentStateMachine::IsAllowed(EquipmentCommand cmd) const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return NextState(m_state, cmd).has_value();
    }

    void EquipmentStateMachine::SetListener(TransitionListener listener)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listener = std::move(listener);
    }

    TransitionResult EquipmentStateMachine::RequestTransition(EquipmentCommand cmd,
                                                              std::wstring_view requester,
                                                              std::wstring_view reason)
    {
        TransitionResult result;
        TransitionListener listener;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            result.from = m_state;
            const auto next = NextState(m_state, cmd);
            if (next.has_value())
            {
                result.allowed = true;
                result.to = *next;
                result.stateChanged = (*next != m_state);
                m_state = *next;
            }
            else
            {
                result.allowed = false;
                result.to = m_state;
                result.stateChanged = false;
                result.reason = std::wstring(L"명령 '") + std::wstring(ToString(cmd))
                              + L"' 은(는) 상태 '" + std::wstring(ToString(m_state)) + L"' 에서 허용되지 않음";
            }
            listener = m_listener;   // 잠금 밖에서 호출하기 위해 복사
        }
        if (listener)
            listener(result, requester, reason);

        if (result.allowed && result.stateChanged)
        {
            rs::core::Event e;
            e.topic = StateChangedTopic;
            e.payload = ToUtf8(result.to);
            e.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            rs::core::EventBus::Instance().Publish(e);
        }

        return result;
    }
}
