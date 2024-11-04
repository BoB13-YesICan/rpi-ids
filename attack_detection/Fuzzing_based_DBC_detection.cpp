#include "Fuzzing_based_DBC_detection.h"

uint64_t extractBits(uint64_t data, int start, int length) {
    int adjustedStart = 64 - start - length;
    uint64_t mask = ((1ULL << length) - 1) << adjustedStart;
    return (data & mask) >> adjustedStart;
}

uint64_t toLittleEndian(uint64_t data, int byteSize) {
    uint64_t result = 0;

    for (int i = 0; i < byteSize; ++i) {
        result |= ((data >> (8 * i)) & 0xFF) << (8 * (byteSize - 1 - i));
    }

    return result;
}

bool check_similarity_with_previous_packet(uint32_t can_id, uint8_t data[8], int DLC, uint8_t valid_payload[8], bool& is_initial_data, int percent) {   
    
    double total_same_percent=0;
    int total_length=0;

    uint64_t payload_combined = 0;
    uint64_t valid_payload_combined = 0;
    for (int i = 0; i < DLC; ++i) {
        payload_combined |= ((uint64_t)data[i] << (8 * (DLC - 1 - i)));
        valid_payload_combined |= ((uint64_t)valid_payload[i] << (8 * (DLC - 1 - i)));
    }
    
    if(message.find(can_id) != message.end()) {
    if(is_initial_data){
        is_initial_data = false;
	    printf("[?] [%03x] [Medium] is_initial_data가 false이여서.. 이게 뭔지몰라서 설명을 못적겠음", can_id);
        return true;
    }
    for (const auto& signal : message[can_id].signals) {
        total_length += signal.length;
        uint64_t old_value, new_value;
        if (signal.byte_order == 1 && signal.length > 8) {
            int first = signal.start_bit / 8;
            int end = ((signal.start_bit + signal.length) / 8);
            int bit_length = (end - first) * 8;
            new_value = extractBits(payload_combined, first * 8, bit_length);
            old_value = extractBits(valid_payload_combined, first * 8, bit_length);
            new_value = toLittleEndian(new_value, (end - first));
            old_value = toLittleEndian(old_value, (end - first));
            new_value = extractBits(new_value, ((end - first) * 8) - signal.length - (signal.start_bit % 8), signal.length);
            old_value = extractBits(old_value, ((end - first) * 8) - signal.length - (signal.start_bit % 8), signal.length);
        } else{
            new_value = extractBits(payload_combined, signal.start_bit, signal.length);
            old_value = extractBits(valid_payload_combined, signal.start_bit, signal.length);
        }
        if (old_value == new_value) {
            total_same_percent += signal.length * 100;
        }else{
            //double diff = (fabs((double)old_value - (double)new_value) / (std::pow(2,total_length)-1) * 100);
            double diff = (fabs((double)old_value - (double)new_value) / (std::max((double)old_value, (double)new_value) * 100));
            total_same_percent += signal.length * (100 - diff);
        }
    }
    if ((total_same_percent / total_length) >= percent) { //수용치
	    printf("[?] [%03x] [Medium] 이전 패킷간의 유사성이 %d%만큼 낮습니다.", can_id, total_same_percent/total_length);
        return true;
    } else {
        return false;
    }
}
  return false;
}

bool validation_check(uint32_t can_id, uint8_t* data, int DLC) {

    uint64_t payload_combined = 0;
    
    for (int i = 0; i < DLC; ++i) {
        payload_combined |= ((uint64_t)data[i] << (8 * (DLC - 1 - i)));
    }

    
    if (message.find(can_id) != message.end()) {
        if (DLC == message[can_id].dlc){
            if (!message[can_id].Skipable) {
                for (const auto& signal : message[can_id].signals) {
                    int start_bit = signal.start_bit;
                    int length = signal.length;
                    int first = start_bit / 8;
                    int end = ((start_bit + length) / 8);
                    uint64_t binary_value = 0;
                    if (signal.byte_order == 1 && (end - first) > 1) {
                        binary_value = extractBits(payload_combined, first * 8, (end - first) * 8);
                        binary_value = toLittleEndian(binary_value, (end - first));
                        binary_value = extractBits(binary_value, ((end - first) * 8) - length - (start_bit % 8), length);
                    } else {
                        binary_value = extractBits(payload_combined, start_bit, length);
                    }
                    if ((uint64_t)signal.LowMinValue <= binary_value && binary_value <= (uint64_t)signal.LowMaxValue) {
			            printf("[?] [%03x] [High] DBC파일의 정의역에 존재하지 않은 페이로드가 아닙니다. Fuzzing 혹은 DoS 공격입니다.\n");
                        return true;
                    } else {
                        return false;
                    }
                }
            } else {
                printf("[?] [%03x] [High] DBC파일의 정의된 ID가 아닙니다. Fuzzing 혹은 DoS 공격입니다.");
                return true;
            }
        }
    }
    return false;
}
