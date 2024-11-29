#include "DoS_detection.h"

double DoS_last_time = 0;  

bool check_DoS(const EnqueuedCANMsg& dequeuedMsg) {
    CANStats& stats = can_stats[dequeuedMsg.can_id];
    double time_diff = dequeuedMsg.timestamp - stats.last_timestamp;

    if (time_diff < DoS_TIME_THRESHOLD_MS) {
        if (memcmp(stats.last_data, dequeuedMsg.data, sizeof(stats.last_data)) == 0) {
            stats.dos_count++;
        } else {
            stats.dos_count = 1;
        }
        if (stats.dos_count == DoS_DETECT_THRESHOLD) {
            is_Attack = 1;
            memset(stats.dos_payload, 0, sizeof(stats.dos_payload));
            memcpy(stats.dos_payload, dequeuedMsg.data, sizeof(dequeuedMsg.data));
	        printf("[DoS] [%03x] [High] 5번 이상 %d개 패킷이 5ms이내 로 빠르게 동일한 페이로드로 수신되었습니다.\n", dequeuedMsg.can_id, stats.dos_count);
            return true;
        }
    }

    if(memcmp(stats.dos_payload, dequeuedMsg.data, sizeof(dequeuedMsg.data)) == 0){
        printf("[DoS] [%03x] [High] 5ms 이내로 빠르게 들어오지는 않았지만 이전까지의 DoS Attack으로 판명된 페이로드로 비정상 주기로 수신되었습니다.\n",dequeuedMsg.can_id);
        DoS_last_time = dequeuedMsg.timestamp;
        return true;
    }
    return false;
}

