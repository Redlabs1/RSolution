#pragma once

#include "core/domain/EquipmentState.h"
#include "ui_winui/viewmodels/MachineUiSnapshot.h"

// 로드맵: "MachineUiSnapshot → EquipmentState 정렬". 도메인 상태(EquipmentState, 8종)를
// UI 표시용 단순 상태(UiMachineState, 6종)로 정렬한다. UI는 세부 도메인 상태를 몰라도
// 되므로 여기서 다대일로 축약한다.
//
// 매핑 근거:
//   PowerOff      -> Stopped       (미기동)
//   Initializing  -> Initializing
//   Idle          -> Ready         (운전 대기 = 시작 가능)
//   AutoRunning   -> Running
//   Paused        -> Idle          (일시정지 = 처리 중단, 대기 취급)
//   Stopping      -> Running       (정지 절차 진행 중 = 아직 활성)
//   Alarm         -> Alarm
//   Maintenance   -> Stopped       (자동 운전 대상 아님)
namespace rs::ui
{
    constexpr UiMachineState MapState(rs::domain::EquipmentState s) noexcept
    {
        using D = rs::domain::EquipmentState;
        switch (s)
        {
        case D::PowerOff:     return UiMachineState::Stopped;
        case D::Initializing: return UiMachineState::Initializing;
        case D::Idle:         return UiMachineState::Ready;
        case D::AutoRunning:  return UiMachineState::Running;
        case D::Paused:       return UiMachineState::Idle;
        case D::Stopping:     return UiMachineState::Running;
        case D::Alarm:        return UiMachineState::Alarm;
        case D::Maintenance:  return UiMachineState::Stopped;
        default:              return UiMachineState::Idle;
        }
    }

    // snapshot.state 만 갱신한다. 알람/레시피/로트 등 나머지 필드는 별도 소스(AlarmService,
    // RecipeRepository 등)에서 채워야 한다.
    inline void ApplyState(MachineUiSnapshot& snapshot, rs::domain::EquipmentState s)
    {
        snapshot.state = MapState(s);
    }
}
