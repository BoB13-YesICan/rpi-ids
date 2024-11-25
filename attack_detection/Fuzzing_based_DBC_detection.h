#ifndef FUZZING_BASED_DBC_DETECTION_H
#define FUZZING_BASED_DBC_DETECTION_H

#include "CANStats.h"
#include "dbcparsed_dbc.h"

#include <unordered_set>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <bitset>

// 함수 선언 (정의는 Fuzzing_based_DBC_detection.cpp 파일에서 한 번만)
uint64_t extractBits(uint64_t data, int start, int length);
uint64_t toLittleEndian(uint64_t data, int byteSize);
bool validation_check(uint32_t can_id, uint8_t* data, int DLC);
bool check_similarity_with_previous_packet(uint32_t can_id, uint8_t data[8], int DLC, uint8_t valid_payload[8], int percent, int& count);
void calc_similarity(uint32_t can_id, uint8_t data[8], int DLC, uint8_t valid_payload[8], int& similarity_percent, int count);

#endif // FUZZING_BASED_DBC_DETECTION_H
