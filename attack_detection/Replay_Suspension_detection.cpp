#include "Replay_Suspension_detection.h"

bool check_replay(CANStats& stats, uint8_t data[]){
    if(memcmp(stats.suspected_payload, data, sizeof(*data)) == 0){
        stats.suspected_count++;
        return stats.suspected_count >= 5;
    } else if(stats.suspected_count > 0){
        stats.suspected_count--;
    } else if(stats.suspected_count <= 0){
        memcpy(stats.suspected_payload, data, sizeof(*data));
        stats.suspected_count = 1; 
    }
    return false;
}


bool check_over_double_periodic(double timestamp, CANStats& stats,uint32_t can_id){
    if(timestamp - stats.last_timestamp > stats.periodic * 5 && timestamp - stats.last_timestamp > 3) {
        printf("[Suspension Attack Reason] %03x packet with a cycle time of %f minute arrived %f minutes later than the previous one. >> ", can_id, stats.periodic, timestamp - stats.last_timestamp);
        return true;
    }
    return false;
}

bool check_malicous_uds(CANStats& stats, uint8_t data[]){
	if(is_uds){
		if(dangerous_uds){
			if(check_dbc){
				if(is_running_car){
					return false;
				}
			} else {
				if(uds_count >10) {
					return false;
				}else{
					if(prev_uds_data == data){
						uds_count++;
					}
				}
			}
		}
	}
	return true;
}
