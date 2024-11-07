#include "DoS_detection.h"

uint8_t DoS_payload[8];
uint32_t DoS_can_id = 0;
float DoS_last_time = 0;  
std::map<uint32_t, int> debugging_dos;
// ANSI 코드로 줄 위치 이동
void move_cursor_up(int lines) {
    std::cout << "\033[" << lines << "A";
}

bool check_DoS(const EnqueuedCANMsg& dequeuedMsg) {
    CANStats& stats = can_stats[dequeuedMsg.can_id];
    double time_diff = dequeuedMsg.timestamp - stats.last_timestamp;

    if (time_diff < DoS_TIME_THRESHOLD_MS) {
        if (memcmp(stats.last_data, dequeuedMsg.data, sizeof(stats.last_data)) == 0) {
            stats.suspected_count++;
        } else {
            stats.suspected_count = 1;
        }
        if (stats.suspected_count == DoS_DETECT_THRESHOLD) {
            DoS_can_id = dequeuedMsg.can_id;
            memcpy(DoS_payload, dequeuedMsg.data, sizeof(DoS_payload));

	    if(debugging_dos.find(dequeuedMsg.can_id)==debugging_dos.end()){
		    debugging_dos[dequeuedMsg.can_id]=1;
		    std::cout << "[DoS Attack] [" << std::hex << dequeuedMsg.can_id 
			    << "] [High] 동일한 페이로드가 5번 이상 5ms 이내로 수신되었습니다. 감지횟수: "
			    << debugging_dos[dequeuedMsg.can_id]<<"번\n";
	    }else{
		    debugging_dos[dequeuedMsg.can_id]++;
		    move_cursor_up(debugging_dos.size());
		    std::cout << "[DoS Attack] [" << std::hex << dequeuedMsg.can_id 
			    << "] [High] 동일한 페이로드가 5번 이상 5ms 이내로 수신되었습니다. 감지횟수: "
			    << debugging_dos[dequeuedMsg.can_id]<<"번\n";
		    std::cout.flush();
	    }
            return true;
        }
    }

    if(DoS_can_id == dequeuedMsg.can_id && memcmp(DoS_payload, dequeuedMsg.data, sizeof(DoS_payload)) == 0){
	    debugging_dos[dequeuedMsg.can_id]++;
	    move_cursor_up(debugging_dos.size());
	    std::cout << "[DoS Attack] [" << std::hex << dequeuedMsg.can_id 
		    << "] [High] 동일한 페이로드가 5번 이상 5ms 이내로 수신되었습니다. 감지횟수: "
		    << debugging_dos[dequeuedMsg.can_id]<<"번\n";
	    std::cout.flush();
	    DoS_last_time = dequeuedMsg.timestamp;
	    return true;
    }
    return false;
}

