#pragma once

#include "core/domain/EquipmentState.h"
#include "ui_winui/viewmodels/MachineUiSnapshot.h"

// UI 문서 v2 5.2: 장비 상태 머신 ↔ UI 표시 상태 표준 매핑.
// v2 에서 UiMachineState 에 Paused / Stopping / Maintenance 가 추가되어 1:1 로 정렬되었다.
//
//   PowerOff      -> Stopped       (표시 대상 아님 — 프로그램 미기동에 준함)
//   Initializing  -> Initializing  (회색 + 진행 표시, "INITIALIZING")
//   Idle          -> Ready         (녹색, "READY")
//   AutoRunning   -> Running       (파란색, "RUN")
//   Paused        -> Paused        (노란색, "PAUSED")
//   Stopping      -> Stopping      (노란색 + 진행 표시, "STOPPING")
//   Alarm         -> Alarm         (빨간색, "ALARM")
//   Maintenance   -> Maintenance   (보라색, "MAINT")
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
        case D::Paused:       return UiMachineState::Paused;
        case D::Stopping:     return UiMachineState::Stopping;
        case D::Alarm:        return UiMachineState::Alarm;
        case D::Maintenance:  return UiMachineState::Maintenance;
        default:              return UiMachineState::Idle;
        }
    }

    // Header 상태 배지 표기 (UI 문서 5.2 "Header 표기 예시").
    constexpr wchar_t const* DisplayText(UiMachineState s) noexcept
    {
        switch (s)
        {
        case UiMachineState::Initializing: return L"INITIALIZING";
        case UiMachineState::Ready:        return L"READY";
        case UiMachineState::Running:      return L"RUN";
        case UiMachineState::Paused:       return L"PAUSED";
        case UiMachineState::Stopping:     return L"STOPPING";
        case UiMachineState::Alarm:        return L"ALARM";
        case UiMachineState::Maintenance:  return L"MAINT";
        case UiMachineState::Stopped:      return L"STOPPED";
        case UiMachineState::Idle:         return L"IDLE";
        default:                           return L"-";
        }
    }

    // App.xaml 상태 Brush 리소스 키 (UI 문서 5장/5.2 표시 색상).
    constexpr wchar_t const* BrushKey(UiMachineState s) noexcept
    {
        switch (s)
        {
        case UiMachineState::Initializing: return L"DisabledBrush";
        case UiMachineState::Ready:        return L"ReadyBrush";
        case UiMachineState::Running:      return L"RunBrush";
        case UiMachineState::Paused:       return L"WarningBrush";
        case UiMachineState::Stopping:     return L"WarningBrush";
        case UiMachineState::Alarm:        return L"AlarmBrush";
        case UiMachineState::Maintenance:  return L"MaintBrush";
        case UiMachineState::Stopped:      return L"DisabledBrush";
        case UiMachineState::Idle:         return L"IdleBrush";
        default:                           return L"IdleBrush";
        }
    }

    // snapshot.state 만 갱신한다. 알람/레시피/로트 등 나머지 필드는 별도 소스(AlarmService,
    // RecipeRepository 등)에서 채워야 한다.
    inline void ApplyState(MachineUiSnapshot& snapshot, rs::domain::EquipmentState s)
    {
        snapshot.state = MapState(s);
    }
}
