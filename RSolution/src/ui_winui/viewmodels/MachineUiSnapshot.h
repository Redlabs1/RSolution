#pragma once

#include <string>

// UI 문서 v2 5.2: 장비 상태 머신(설계서 7.1, 8종)과 1:1 대응하는 UI 표시 상태.
// 상태 판단의 기준은 항상 EquipmentStateMachine 이며, 이 열거는 표시용이다.
enum class UiMachineState
{
    Idle,
    Initializing,
    Ready,
    Running,
    Paused,       // (v2 추가) 설계서 Paused
    Stopping,     // (v2 추가) 설계서 Stopping
    Alarm,
    Maintenance,  // (v2 추가) 설계서 Maintenance
    Stopped
};

struct MachineUiSnapshot
{
    UiMachineState state{ UiMachineState::Idle };
    bool hasActiveAlarm{ false };
    std::string activeAlarmText{};
    std::string currentLot{};
    std::string currentRecipe{};
    bool engineerMode{ false };
};
