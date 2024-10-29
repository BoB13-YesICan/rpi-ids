#ifndef DBC_H
#define DBC_H

#include <json/json.h>
#include <unordered_set>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <bitset>
#include "CANStats.h"


extern Json::Value dbc;
extern Json::CharReaderBuilder readerBuilder;
extern std::string errs;

void read_dbc(const std::string& filename);

unsigned long long get_bits_from_hex_string(const std::string &hex_string, int start_bit, int bit_length);
unsigned long long cut_bits(unsigned long long number, int start, int length);
unsigned long long to_little_endian_int(unsigned long long number, int byte_size);
bool is_binary_string_valid(const std::string &binary_string);
bool parse_csv_line(const std::string &line, std::vector<std::string> &data);

bool validation_check(const Json::Value& dbc, uint32_t can_id, uint8_t data[8], int DLC);
bool check_similarity_with_previous_packet(const Json::Value& dbc, uint32_t can_id, uint8_t data[8], int DLC, uint8_t valid_payload[8], bool is_initial_data);

//int total_same_percent;
//int total_lentgh;
#endif // DBC_COMPARE_H
