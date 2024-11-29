#include "Replay_Suspension_detection.h"
#include "DoS_detection.h"
bool check_replay(CANStats& stats, uint8_t data[], uint32_t can_id){
    if(memcmp(stats.replay_payload, data, sizeof(*data)) == 0){
        stats.replay_count++;
        if(stats.replay_count >= 5){
            is_Attack = 2;
            printf("[Replay] [%03x] [Medium] PE 메세지는 아니나, 정상 주기 패킷 직전에 동일한 페이로드의 패킷이 비정상적인 주기를 가지고 %d개 수신되었습니다.\n", can_id, stats.replay_count);
            return true;
        }
    } else if(stats.replay_count > 0){
        stats.replay_count--;
    } else if(stats.replay_count <= 0){
        memcpy(stats.replay_payload, data, sizeof(*data));
        stats.replay_count = 1; 
    }
    return false;
}


bool check_over_double_periodic(double timestamp, CANStats& stats,uint32_t can_id){
    if(timestamp - stats.last_timestamp > stats.periodic * 5 && timestamp - stats.last_timestamp > 3) {
        if(timestamp - DoS_last_time < 10) return false;  
            printf("[Suspension] [%03x] [High] %.6f 주기로 들어오는 %03x 패킷가 %.6fms만큼 더 늦게 수신되었습니다.\n", can_id, stats.periodic, can_id, timestamp - stats.last_timestamp);
            return true;
        }
    return false;
}