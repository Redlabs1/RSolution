#pragma once
#include <string>

struct MachineSnapshot
{
    //MachineState state;
    bool servoReady;
    bool vacuumOn;
    std::string alarm;
};
