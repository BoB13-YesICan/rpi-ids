#include "CANStats.h"

// Define the unordered_map
std::unordered_map<uint32_t, CANStats> can_stats;
std::set<uint32_t> sorted_canIDs;

uint32_t MIN_CAN_ID = 0;
