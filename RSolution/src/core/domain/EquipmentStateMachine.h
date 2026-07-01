#pragma once

#include "core/domain/EquipmentState.h"
#include "core/domain/EquipmentCommand.h"
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

// 설계서 7.1.5 / 7.1.8: 모든 상태 전이는 이 클래스의 단일 진입점(RequestTransition)에서만 수행한다.
// SequenceManager, AlarmManager, HostComm, UI 는 상태 값을 직접 변경하지 않는다.
namespace rs::domain
{
    // 상태가 실제로 바뀔 때(RequestTransition 이 허용되고 stateChanged=true 일 때) EventBus 로
    // 발행되는 토픽. payload = 새 상태 이름(UTF-8, EquipmentState::ToUtf8 참고).
    // 설계서 로드맵 "UI 연동 — ViewModel ↔ 도메인을 IEventBus 로 연결".
    inline constexpr char StateChangedTopic[] = "state.changed";

    struct TransitionResult
    {
        bool           allowed{ false };
        EquipmentState from{ EquipmentState::PowerOff };
        EquipmentState to{ EquipmentState::PowerOff };   // allowed=false 면 from 과 동일
        bool           stateChanged{ false };            // 허용되었으나 상태 유지(예: Manual)면 false
        std::wstring   reason;                           // 거부 사유 등
    };

    class EquipmentStateMachine
    {
    public:
        EquipmentStateMachine() = default;

        // 애플리케이션 전역 상태머신. AlarmService 등 다른 싱글톤 구성요소가 상태 전이를
        // 요청할 때 이 인스턴스를 사용한다(설계서 7.1.5 단일 진입점).
        static EquipmentStateMachine& Instance() noexcept;

        EquipmentState State() const noexcept;

        // 현재 상태에서 명령 허용 여부(7.1.6).
        bool IsAllowed(EquipmentCommand cmd) const noexcept;

        // 유일한 상태 전이 진입점(7.1.5). 허용 시 전이(상태 유지 포함)하고 결과를 반환한다.
        // requester: "UI"/"Host"/"Sequence"/"Alarm" 등, reason: 전이 사유.
        TransitionResult RequestTransition(EquipmentCommand cmd,
                                           std::wstring_view requester,
                                           std::wstring_view reason);

        // 전이 시도마다 호출되는 리스너(로그/이벤트 발행용). 이전/다음 상태, 요청자, 사유, 허용 여부 전달.
        using TransitionListener =
            std::function<void(TransitionResult const&, std::wstring_view requester, std::wstring_view reason)>;
        void SetListener(TransitionListener listener);

    private:
        // 전이 테이블(7.1.4/7.1.6). 허용되지 않으면 nullopt, 허용되면 목표 상태(상태 유지면 현재와 동일).
        static std::optional<EquipmentState> NextState(EquipmentState s, EquipmentCommand cmd) noexcept;

        mutable std::mutex m_mutex;
        EquipmentState     m_state{ EquipmentState::PowerOff };
        TransitionListener m_listener;
    };
}
