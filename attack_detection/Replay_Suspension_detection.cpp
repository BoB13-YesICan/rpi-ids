#include "Replay_Suspension_detection.h"
#include "DoS_detection.h"
bool check_replay(CANStats& stats, uint8_t data[], uint32_t can_id){
    if(memcmp(stats.suspected_payload, data, sizeof(*data)) == 0){
        stats.suspected_count++;
	//printf("[Replay] [%03x] [Medium] PE 메세지는 아닌데, 정상 주기 패킷 직전에 동일한 페이로드의 패킷이 비정상적인 주기를 가지고 %d개 수신되었습니다.", can_id, stats.suspected_count);
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
	if(timestamp - DoS_last_time < 10) return false;  
        printf("[Suspension] [%03x] [High] %f 주기로 들어오는 %03x 패킷가 %fms만큼 더 늦게 수신되었습니다.", can_id, stats.periodic, can_id, timestamp - stats.last_timestamp);
        return true;
    }
    return false;
}
