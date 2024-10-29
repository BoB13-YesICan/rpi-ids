#include "attack_detection_with_dbc.h"
#include "CANStats.h"

extern int under_attack;
uint32_t last_can_id = 0; 
int consecutive_count = 0;
uint32_t last_can_data[8] ={0,};

bool check_periodic_range(double time_diff, double periodic){
    return (periodic * 0.8 <= time_diff && time_diff <= periodic * 1.2);
}


bool check_previous_packet_of_avg(double current_timediff, CANStats& stats){
    if(stats.prev_timediff == 0.0){
        stats.prev_timediff = current_timediff;
        return false;
    }

    if(check_periodic_range((current_timediff + stats.prev_timediff) / 2, stats.periodic)){
        // printf("current_timediff: %.6f prev_timediff: %.6f periodic: %.6f\n", current_timediff, stats.prev_timediff, stats.periodic);
        stats.prev_timediff = 0;
        return true;
    }

    stats.prev_timediff = 0;
    return false;
}

bool check_low_can_id(uint32_t can_id){
	//only check lowest can id
    return (can_id <= MIN_CAN_ID);
}

bool check_DoS(EnqueuedCANMsg dequeuedMsg){
    CANStats& stats = can_stats[dequeuedMsg.can_id];
    double time_diff = (dequeuedMsg.timestamp - stats.last_timestamp);

    if(time_diff < DoS_TIME_THRESHOLD_MS){
        if(memcmp(stats.last_data, dequeuedMsg.data, sizeof(stats.last_data)) == 0){
            stats.suspected_count++;
        } else{ 
            stats.suspected_count = 1;
        }

        if(stats.suspected_count == DoS_DETECT_THRESHOLD){
            DoS_can_id = dequeuedMsg.can_id;
            memcpy(DoS_payload, dequeuedMsg.data, sizeof(DoS_payload));
            return true;
        }
    }

    if(DoS_can_id == dequeuedMsg.can_id && memcmp(DoS_payload, dequeuedMsg.data, sizeof(DoS_payload)) == 0){
        return true;
    }

    return false;
}

bool check_onEvent(double timestamp, CANStats& stats, uint32_t can_id, uint8_t data[]){
	double event_time_diff = 0;
    double time_diff = timestamp - stats.last_normal_timestamp;

    if(stats.event_count == -1){
        memset(stats.event_payload, 0, sizeof(stats.event_payload));
        stats.event_count = 0;
        stats.event_last_timestamp = timestamp;
    } else {
        event_time_diff = timestamp - stats.event_last_timestamp;
        if(EVENT_PERIOD * 1.2 >= event_time_diff && EVENT_PERIOD * 0.8 <= event_time_diff){
            stats.event_count++;
            stats.event_last_timestamp = timestamp;
            memcpy(stats.event_payload, data, sizeof(stats.event_payload));

            if(stats.event_count <= 10 && stats.event_count >= 1) {
		    printf("%d Count Size\n",stats.event_count);
                return true;
            }
            else {
                printf("[On-Event] %03x so many packet with short time(30ms)\n",can_id);
                stats.event_count = -1;
                return false;
            }
        } else if(check_periodic_range(time_diff / 2, stats.periodic) || memcmp(stats.event_payload, data, sizeof(stats.event_payload)) == 0){
            stats.event_count = -1;
            stats.last_normal_timestamp = timestamp;
            return true;
        } else {
            stats.event_count = -1;
        }
    }
    return false;
}

bool check_over_double_periodic(double timestamp, CANStats& stats,uint32_t can_id){
	if(timestamp - stats.last_timestamp > stats.periodic * 5) {
		printf("[Suspension Attack Reason] %03x packet with a cycle time of %f minute arrived %f minutes later than the previous one. >> ", can_id, stats.periodic, timestamp - stats.last_timestamp);
		return true;  
	}	
	return false;
}


bool filtering_process(EnqueuedCANMsg* dequeuedMsg) {
    bool malicious_packet = true;
    bool normal_packet = false;
    
    CANStats& stats = can_stats[dequeuedMsg->can_id];
    if(dbc_check && !validation_check(dequeuedMsg->can_id,dequeuedMsg->data,dequeuedMsg->DLC)){
	    printf("Fuzzing or Relay : Not match with DBC %03x\n", dequeuedMsg->can_id);
	    return malicious_packet;
    }
    // 비주기 패킷일 경우
    if (!stats.is_periodic||stats.count<=1) {
        if (!check_similarity_with_previous_packet(dequeuedMsg->can_id, dequeuedMsg->data, dequeuedMsg->DLC, stats.valid_last_data, stats.is_initial_data, 83)) {
                // Fuzzing or Replay 공격
	        printf("%03x DBC Fuzzing or Replay\n", dequeuedMsg->can_id);
                return malicious_packet;
            }
	    if (check_low_can_id(dequeuedMsg->can_id)) {
	            //printf("not period\n");
		    if((check_DoS(*dequeuedMsg))){
			 printf("%03x DoS Attack\n", dequeuedMsg->can_id);
	    		 return malicious_packet;
		    }
	    }
        memcpy(stats.valid_last_data, dequeuedMsg->data, sizeof(dequeuedMsg->data));
        // 비주기 패킷은 정상 패킷으로 처리
        return normal_packet;
    }
    // 주기 패킷일 경우
    // 1. 정상 주기 범위 내에 있는가? 또는 i-1 패킷과의 평균값이 정상 주기 범위 내에 있는가?
    double time_diff = dequeuedMsg->timestamp - stats.last_timestamp;
    // printf("time_diff: %.6f\n", time_diff);
    if (check_periodic_range(time_diff, stats.periodic) || check_previous_packet_of_avg(time_diff, stats)) {
        // 1.1 이전 패킷과 상관관계가 있는가?
            // 1.2 시계 오차가 있는가?
            if (!check_clock_error(dequeuedMsg->can_id, dequeuedMsg->timestamp)) {
                // 정상 패킷
                memcpy(stats.valid_last_data, dequeuedMsg->data, sizeof(dequeuedMsg->data));
                return normal_packet;
            } else {
                // Masquerade 공격
		printf("%03x Masquarade attack \n",dequeuedMsg->can_id);
		return malicious_packet;
            }
    }
    // 2.1 최하위 CAN ID인가?
    if (check_low_can_id(dequeuedMsg->can_id)) {
        // 오차가 5ms 이내로 동일한 패킷이 5번 이상 들어오는가?
        if (check_DoS(*dequeuedMsg)) {
            // DDoS 공격
	    printf("%03x Dos Attack\n", dequeuedMsg->can_id);
            return malicious_packet;
	    }
    }
    if (!check_similarity_with_previous_packet(dequeuedMsg->can_id, dequeuedMsg->data, dequeuedMsg->DLC, stats.valid_last_data, stats.is_initial_data, 83)) {
                // Fuzzing or Replay 공격
	        printf("%03x DBC Fuzzing or Replay\n", dequeuedMsg->can_id);
                return malicious_packet;
            }
    // 2.2 On-Event 패킷인가?
    if (check_onEvent(dequeuedMsg->timestamp, stats,dequeuedMsg->can_id, dequeuedMsg->data)) {
        // 정상 패킷
        memcpy(stats.valid_last_data, dequeuedMsg->data, sizeof(dequeuedMsg->data));
        printf("Event ID: %03x\n",dequeuedMsg->can_id);
        return normal_packet;
    }

    // 2.3 오차가 정상 주기의 2배 이상인가?
    if (check_over_double_periodic(dequeuedMsg->timestamp, stats, dequeuedMsg->can_id)) {
	// Suspension 공격
	printf("%03x Suspenstion\n", dequeuedMsg->can_id);
        return malicious_packet;
    }

    if(stats.event_count >= 0 || stats.prev_timediff != 0){
        return normal_packet;
    }
    return normal_packet;
}

uint8_t DoS_payload[8];    // 전역 변수 정의
int suspected_count = 0;   // 전역 변수 정의
uint32_t DoS_can_id = 0;   // 전역 변수 정의