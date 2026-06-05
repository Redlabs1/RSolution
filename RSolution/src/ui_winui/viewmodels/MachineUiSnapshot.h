#pragma once

#include <string>

enum class UiMachineState
{
    Idle,
    Initializing,
    Ready,
    Running,
    Alarm,
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
