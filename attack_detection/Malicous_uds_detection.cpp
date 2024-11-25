#include "Malicous_uds_detection.h"

const std::vector<uint8_t> VALID_UDS_REQUESTS = {0x10, 0x11, 0x22, 0x2E, 0x2F, 0x31, 0x34, 0x37};
const std::uint16_t VALID_CAN_ID_MIN = 0x700;
const std::uint16_t VALID_CAN_ID_MAX = 0x7FF;
const uint8_t RESET_SERVICE_CODE = 0x11; // ECU Reset service code
const int HIGH_FREQUENCY_THRESHOLD = 10;
const double COUNT_THRESHOLD = 0.5;

bool isValidUDS(uint8_t data[],  uint32_t can_id){
	if(can_id < VALID_CAN_ID_MIN || can_id > VALID_CAN_ID_MAX){
		return false;
	}

	//dlc?
	//printf("%02x%02x%02x\n", data[0],data[1],data[2]);
	if(std::find(VALID_UDS_REQUESTS.begin(), VALID_UDS_REQUESTS.end(), data[1]) != VALID_UDS_REQUESTS.end()) {
		//printf("Malicous UDS %03x \n", can_id);
		return true;
	}

	return false;
}

bool isMalicousUDS(CANStats& stats, uint8_t data[], uint32_t can_id){
	if(isValidUDS(data, can_id)){
		if(data[1] == RESET_SERVICE_CODE){
			if(stats.resetcount ==0){
				stats.resetcount++;
			} else {
				if(stats.reset_timestamp - stats.last_timestamp < HIGH_FREQUENCY_THRESHOLD){
					stats.resetcount += 3;
				}else {
					stats.resetcount = 0;
				}
			}
		}
		if(stats.resetcount ==1){
			stats.reset_timestamp = stats.last_timestamp;
		}
		if(stats.resetcount >= COUNT_THRESHOLD){
			printf("Malcious UDs %03x\n", can_id);
			return true;
		}
		return false;
	}
	return false;
}
