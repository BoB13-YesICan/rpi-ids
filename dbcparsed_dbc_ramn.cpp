#include <unordered_map>
#include <string>
#include <vector>
#include "dbcparsed_dbc.h"
std::unordered_map<int, CANMessage> message = {
    {0x24, {true, "Brake", 8, "Vector__XXX", {
        {"Counter", 23, 16, 0, 0, 0, 65535},
        {"Value", 3, 12, 0, 0, 0, 4095}
    }}},
    {0x39, {true, "Accelerator", 8, "Vector__XXX", {
        {"Counter", 23, 16, 0, 0, 0, 65535},
        {"Value", 3, 12, 0, 0, 0, 4095}
    }}},
    {0x62, {true, "Steering", 8, "Vector__XXX", {
        {"Counter", 23, 16, 0, 0, 0, 65535},
        {"Value", 3, 12, 0, 0, 0, 4095}
    }}},
    {0x77, {true, "Shift", 8, "Vector__XXX", {
        {"Counter", 23, 16, 0, 0, 0, 65535},
        {"JoystickPosition", 15, 8, 0, 0, 0, 255},
        {"CurrentGear", 7, 8, 0, 1, -128, 127}
    }}},
    {0x98, {true, "Horn", 8, "Vector__XXX", {
        {"Counter", 23, 16, 0, 0, 0, 65535},
        {"Status", 0, 1, 0, 1, 0, 1}
    }}},
    {0x150, {true, "LightSwitch", 8, "Vector__XXX", {
        {"Counter", 23, 16, 0, 0, 0, 65535},
        {"Value", 2, 3, 0, 0, 0, 7}
    }}},
    {0x1a7, {true, "TurnIndicators", 8, "Vector__XXX", {
        {"Counter", 23, 16, 0, 0, 0, 65535},
        {"Left", 8, 1, 0, 1, 0, 1},
        {"Right", 0, 1, 0, 1, 0, 1}
    }}},
    {0x1b8, {true, "EngineKey", 8, "Vector__XXX", {
        {"Counter", 23, 16, 0, 0, 0, 65535},
        {"Status", 1, 2, 0, 0, 0, 3}
    }}},
    {0x1bb, {true, "LEDStatus", 8, "Vector__XXX", {
        {"Counter", 23, 16, 0, 0, 0, 65535},
        {"RightTurn", 7, 1, 0, 1, 0, 1},
        {"LeftTurn", 6, 1, 0, 1, 0, 1},
        {"HighBeam", 5, 1, 1, 1, 0, 1},
        {"LowBeam", 4, 1, 1, 1, 0, 1},
        {"Clearance", 3, 1, 0, 1, 0, 1},
        {"BrakeStop", 2, 1, 1, 1, 0, 1},
        {"CheckEngine", 1, 1, 0, 1, 0, 1},
        {"Battery", 0, 1, 0, 1, 0, 1}
    }}},
    {0x1d3, {true, "ParkingBrake", 8, "Vector__XXX", {
        {"Counter", 23, 16, 0, 0, 0, 65535},
        {"Status", 0, 1, 0, 1, 0, 1}
    }}}
};

