#include "dbc.h"
#include "CANStats.h"
Json::Value dbc;
Json::CharReaderBuilder readerBuilder;
std::string errs;

void read_dbc(const std::string& filename){

    FILE *dbc_file = fopen(filename.c_str(), "r");
    if (dbc_file == NULL) {
        perror("Could not open the period file");
        exit(1);
    }

    // JSON 파일을 읽어서 파싱
    fseek(dbc_file, 0, SEEK_END);
    long length = ftell(dbc_file);
    fseek(dbc_file, 0, SEEK_SET);
    char *jsonData = new char[length + 1];
    fread(jsonData, 1, length, dbc_file);
    jsonData[length] = '\0';

    std::string jsonStr(jsonData);
    std::istringstream jsonStream(jsonStr);

    if (!Json::parseFromStream(readerBuilder, jsonStream, &dbc, &errs)) {
        fprintf(stderr, "Error parsing JSON: %s\n", errs.c_str());
        delete[] jsonData;
        fclose(dbc_file);
        return ;
    }

    delete[] jsonData;
    fclose(dbc_file); // JSON 파일 닫기
}

unsigned long long get_bits_from_hex_string(const std::string &hex_string, int start_bit, int bit_length) {
    unsigned long long value = 0;
    for (char c : hex_string) {
        value <<= 4; 
        value |= (c >= '0' && c <= '9') ? c - '0' : 10 + (tolower(c) - 'a');
    }

    unsigned long long mask = (1ULL << bit_length) - 1;
    return (value >> (hex_string.length() * 4 - start_bit - bit_length)) & mask;
}

unsigned long long cut_bits(unsigned long long number, int start, int length) {
    unsigned long long mask = (1ULL << length) - 1;
    return (number >> (64 - start - length)) & mask;
}

unsigned long long to_little_endian_int(unsigned long long number, int byte_size) {
    unsigned long long result = 0;
    for (int i = 0; i < byte_size; ++i) {
        result |= ((number >> (i * 8)) & 0xFF) << (i * 8);
    }
    return result;
}

bool check_similarity_with_previous_packet(const Json::Value& dbc, uint32_t can_id, uint8_t data[8], int DLC, uint8_t valid_payload[8], bool is_initial_data) {
    if(dbc.isNull()) return true;
    int total_same_percent=0;
    int total_length=0;

    std::stringstream can_id_string;
    can_id_string << std::hex << can_id;
    std::string can_id_hexStr = can_id_string.str();

    std::ostringstream payload_string;
    for (int i = 0; i < DLC; ++i) {
        payload_string << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    // 문자열로 저장
    std::string payload_hexStr = payload_string.str();

    std::ostringstream valid_payload_string;
    for (int i = 0; i < DLC; ++i) {
        valid_payload_string << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    // 문자열로 저장
    std::string valid_payload_hexStr = valid_payload_string.str();
    for (const auto &message : dbc) {
            if ("0x" + can_id_hexStr == message["CAN ID"].asString()) {
    if(is_initial_data){
        is_initial_data = false;
        return true;
    }else{
        for (const auto &signal : message["Signals"]) {
          total_length += signal["Length"].asInt();;
          unsigned long long old_value, new_value;
          if (signal["Byte Order"].asInt() == 1) {
              int first = signal["Start Bit"].asInt() / 8;
              int end = ((signal["Start Bit"].asInt() + signal["Length"].asInt()) / 8);
              int bit_length = (end - first) * 8;
              old_value = get_bits_from_hex_string(valid_payload_hexStr, first * 8, bit_length);
              new_value = get_bits_from_hex_string(payload_hexStr, first * 8, bit_length);
              old_value = std::bitset<64>(to_little_endian_int(old_value, (end - first))).to_ullong();
              new_value = std::bitset<64>(to_little_endian_int(new_value, (end - first))).to_ullong();
              old_value = cut_bits(old_value, ((end - first) * 8) - signal["Length"].asInt() - (signal["Start Bit"].asInt() % 8), signal["Length"].asInt());
              new_value = cut_bits(new_value, ((end - first) * 8) - signal["Length"].asInt() - (signal["Start Bit"].asInt() % 8), signal["Length"].asInt());
          } else {
              old_value = get_bits_from_hex_string(valid_payload_hexStr, signal["Start Bit"].asInt(), signal["Length"].asInt());
              new_value = get_bits_from_hex_string(payload_hexStr, signal["Start Bit"].asInt(), signal["Length"].asInt());
          }
          if (old_value == new_value) {
              total_same_percent += signal["Length"].asInt() * 100;
          } else {
              double diff = abs((double)old_value - (double)new_value) / (double)std::max(old_value, new_value) * 100;
              total_same_percent += signal["Length"].asInt() * (100 - diff);
          }
      }

      if ((total_same_percent / total_length) >= 40) { //수용치
          return true;
      } else {
          return false;
      }
    }}
}
  return true;
}

bool validation_check(const Json::Value& dbc, uint32_t can_id, uint8_t* data, int DLC) {
    bool is_valid_id=false;
    bool is_valid_range_data=false;

    std::stringstream can_id_string;
    can_id_string << std::hex << can_id;
    std::string can_id_hexStr = can_id_string.str();

    std::ostringstream payload_string;
    for (int i = 0; i < DLC; ++i) {
        payload_string << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    // 문자열로 저장
    std::string payload_hexStr = payload_string.str();

    for (const auto &message : dbc) {
        if ("0x" + can_id_hexStr == message["CAN ID"].asString()){ //can_id check
            is_valid_id = true;
            for (const auto &signal : message["Signals"]) {
                if (!signal["Skipable"].asBool()) {
                    int start_bit = signal["Start Bit"].asInt();
                    int length = signal["Length"].asInt();
                    int first = start_bit / 8;
                    int end = ((start_bit + length) / 8);
                    unsigned long long binary_value = 0;
                    if (signal["Byte Order"].asInt() == 1 && (end - first) > 1) {
                        unsigned long long binary_value = get_bits_from_hex_string(payload_hexStr, first * 8, (end - first) * 8);
                        binary_value = to_little_endian_int(binary_value, (end - first));
                        binary_value = cut_bits(binary_value, ((end - first) * 8) - length - (start_bit % 8), length);
                    } else {
                        binary_value = get_bits_from_hex_string(payload_hexStr, start_bit, length);
                    }
                    if (signal["Low Min Value"].asDouble() <= binary_value && binary_value <= signal["Low Max Value"].asDouble()) {
                        return true;
                    } else {
                        return is_valid_range_data;
                    }
                } else {
			return true;
                }
            }
            return true;
        }
    }
    return is_valid_id;
}
