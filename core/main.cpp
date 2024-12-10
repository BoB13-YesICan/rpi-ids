#include "header.h"
#define MAX_LINES 100     // log_print_buffer의 최대 줄 수
#define MAX_LENGTH 1024   // 각 줄의 최대 문자열 길이

std::map<int, std::chrono::steady_clock::time_point> canIdTimers;
std::mutex timerMutex;

std::mutex queueMutex;
std::condition_variable queueCondVar;
bool done = false;
double start_time=0;
struct timeval tv;
int under_attack = 0;
int sum=0;
int susp[2303] = {0,};
int current_row = 0;

void printAsciiArt() {
    std::cout << R"(
 _____   ___   _   _   _____ ______  _____
/  __ \ / _ \ | \ | | |_   _||  _  \/  ___|
| /  \// /_\ \|  \| |   | |  | | | |\ `--.
| |    |  _  || . ` |   | |  | | | | `--. \
| \__/\| | | || |\  |  _| |_ | |/ / /\__/ /
 \____/\_| |_/\_| \_/  \___/ |___/  \____/
    )" << std::endl;
}

void printoneiArt() {
    std::cout << R"(
    
    
    
    
    
    
    
    
    
    
    
======================================================================================
    )" << std::endl;
    initializeAttackTypes();
}

// CAN 메시지 수신 처리 함수 정의
void onCanMessageReceived(int canId) {
    auto currentTime = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(timerMutex);  // 뮤텍스 잠금
    canIdTimers[canId] = currentTime;
}

// 타이머 확인 함수 정의
void timerCheckThread() {
    std::chrono::seconds timeout(5);
    while (true) {
        {
            std::lock_guard<std::mutex> lock(timerMutex);  // 뮤텍스 잠금
            auto currentTime = std::chrono::steady_clock::now();
            for (const auto& pair : canIdTimers) {
                int canId = pair.first;
                auto lastReceivedTime = pair.second;
                CANStats& stats = can_stats[canId];
                timeout = std::chrono::seconds(static_cast<int>(stats.periodic * 10)+3);
                if (currentTime - lastReceivedTime > timeout && stats.is_periodic) {
                    susp[canId] = 1;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

Queue_t canMsgQueue; //CAN 데이터를 담을 큐

uint32_t get_lowest_can_id() {
    if (can_stats.empty()) {
        std::cerr << "No CAN IDs found in the stats." << std::endl;
        return 0; // can_stats가 비어있으면 0을 반환
    }

    uint32_t lowest_can_id = UINT32_MAX; // 처음에는 최대값으로 설정

    for (const auto& entry : can_stats) {
        if (entry.first < lowest_can_id) {
            lowest_can_id = entry.first;
        }
    }

    return lowest_can_id;
}

// 현재 타임스탬프를 초와 마이크로초 단위로 구하는 함수
double get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);  // 현재 시간을 가져옴
    return tv.tv_sec + (tv.tv_usec / 1000000.0);  // 초와 마이크로초를 합쳐서 double로 변환
}

// CAN 패킷을 수신하고 qCANMsg 구조체에 저장하는 함수
int receive_can_frame(int s, EnqueuedCANMsg* msg) {
    bool is_first_packet = true;
    while(!done){
	    struct can_frame frame;

        size_t nbytes = read(s, &frame, sizeof(struct can_frame));
        if (nbytes < 0) {
            perror("CAN read error");
            return -1;
        }

        msg->timestamp = get_timestamp();  // 타임스탬프를 구조체에 저장
        msg->can_id = frame.can_id;
        msg->DLC = frame.can_dlc;
        memcpy(msg->data, frame.data, frame.can_dlc);  // 수신한 데이터 저장

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if(q_push(&canMsgQueue, msg)){
                if(is_first_packet){
                    gettimeofday(&tv, NULL);  
                    start_time = tv.tv_sec + (tv.tv_usec / 1000000.0);
                    is_first_packet = false;
                }
                queueCondVar.notify_one();
            } else{
                printf("Queue is full.\n");
            }
        }
    }
    return 0;
}

// 큐에서 메시지를 꺼내고 처리하는 함수
void process_can_msg(const char *log_filename){
    int packet_limit=110000;
    int packet_cnt=0;
    int log_num=0;

    char log_filename_ver[100]; // 충분히 큰 버퍼 크기를 설정
    sprintf(log_filename_ver, "%s%d", log_filename, log_num);

    FILE *logfile_whole = fopen(log_filename_ver, "w");
    
    // char log_print_buffer[MAX_LINES][MAX_LENGTH];
    char log_print_buffer[MAX_LINES*MAX_LENGTH]="";
    char *log_print_ptr=log_print_buffer;

    char log_buffer[MAX_LENGTH];  // 충분히 큰 버퍼를 미리 할당
    char *log_ptr = log_buffer;  // log_buffer의 시작 위치로 초기화

    char log_temp[MAX_LENGTH];
    int log_index = 0;   

    bool check = true;

    auto log_start_time=std::chrono::steady_clock::now();   

    while(!done){
                 

        std::unique_lock<std::mutex> lock(queueMutex);
        queueCondVar.wait(lock, []{return !q_isEmpty(&canMsgQueue)|| done; });

        while ((!q_isEmpty(&canMsgQueue))){

            EnqueuedCANMsg dequeuedMsg;
            q_pop(&canMsgQueue, &dequeuedMsg);
            
            packet_cnt++;
            if (packet_cnt>packet_limit){
                log_num++;
                sprintf(log_filename_ver, "%s%d", log_filename, log_num);
                logfile_whole = fopen(log_filename_ver, "w");
                packet_cnt=0;
            }
            lock.unlock();
            //첫번째 문자열 작성
            log_ptr +=sprintf(log_ptr, "%.6f can0 %03X#", dequeuedMsg.timestamp, dequeuedMsg.can_id);

            for (int i = 0; i < dequeuedMsg.DLC; i++) {
                log_ptr +=sprintf(log_ptr, "%02X", dequeuedMsg.data[i]);
                // strcat(log_buffer, log_temp);
            }

            CANStats& stats = can_stats[dequeuedMsg.can_id];
          
            if(dequeuedMsg.timestamp - start_time <= 40 && stats.count < 201){
                log_ptr +=sprintf(log_ptr, " 0\n");

                calc_periodic(dequeuedMsg.can_id, dequeuedMsg.timestamp);
                
                #ifdef SET_DBC_CHECK
                calc_similarity(dequeuedMsg.can_id, dequeuedMsg.data, dequeuedMsg.DLC, stats.valid_last_data, stats.similarity_percent, stats.count);
                #endif
            
            } else if(susp[dequeuedMsg.can_id]){
                susp[dequeuedMsg.can_id] = 0;
                stats.event_count = -1;
                stats.prev_timediff = 0;
                
                log_ptr+=sprintf(log_ptr, " 8 periodic: %.6lf time_diff: %.6lf reset_count: %d\n", stats.periodic, dequeuedMsg.timestamp - stats.last_timestamp, stats.resetcount);
                // strcat(log_buffer, log_temp);
            } else if(check){
	            
	            log_ptr+=sprintf(log_ptr, " 0 periodic: %.6lf\n", stats.periodic);
                // strcat(log_buffer, log_temp);


	            #ifdef SET_DBC_CHECK
                calc_similarity(dequeuedMsg.can_id, dequeuedMsg.data, dequeuedMsg.DLC, stats.valid_last_data, stats.similarity_percent, stats.count);
                #endif
                check = false;
            } else {
                int filtering_result = filtering_process(&dequeuedMsg);

                if(stats.last_timestamp == 0)
                    stats.last_timestamp = dequeuedMsg.timestamp;
                
                if (filtering_result == 0){
                    onCanMessageReceived(dequeuedMsg.can_id);
                    #ifdef SET_DBC_CHECK
                    calc_similarity(dequeuedMsg.can_id, dequeuedMsg.data, dequeuedMsg.DLC, stats.valid_last_data, stats.similarity_percent, stats.count);
                    #endif
                    
                    log_ptr+=sprintf(log_ptr, " 0 periodic: %.6lf\n", stats.periodic);
                    // strcat(log_buffer, log_temp);

                } else {
                    stats.event_count = -1;
                    stats.prev_timediff = 0;
                    if(filtering_result == 1 || filtering_result == 2 || filtering_result == 3 || filtering_result == 6){
                        
                        log_ptr+=sprintf(log_ptr, " %d periodic: %.6lf time_diff: %.6lf\n", filtering_result, stats.periodic, dequeuedMsg.timestamp - stats.last_timestamp);
                        // strcat(log_buffer, log_temp);
                    } else if(filtering_result == 4 || filtering_result == 5) {
                        
                        log_ptr+=sprintf(log_ptr, " %d periodic: %.6lf time_diff: %.6lf similarity: %.6lf\n", filtering_result, stats.periodic, dequeuedMsg.timestamp - stats.last_timestamp, stats.similarity_percent);
                        // strcat(log_buffer, log_temp);

                    } else if(filtering_result == 7 || filtering_result == 8){
                        
                        log_ptr+=sprintf(log_ptr, " %d periodic: %.6lf time_diff: %.6lf reset_count: %d\n", filtering_result, stats.periodic, dequeuedMsg.timestamp - stats.last_timestamp, stats.resetcount);
                        // strcat(log_buffer, log_temp);

                    } else {
                        
                        log_ptr+=sprintf(log_ptr, " %d periodic: %.6lf time_diff: %.6lf clock_skew: %.6lf clock_skew_lowerlimit: %.6lf clock_skew_upperlimit: %.6lf\n", filtering_result, stats.periodic, dequeuedMsg.timestamp - stats.last_timestamp, stats.clock_skew, stats.clock_skew_lowerlimit, stats.clock_skew_upperlimit);
                        // strcat(log_buffer, log_temp);

                    }
                }
            }
            fprintf(logfile_whole, log_buffer);
// //모든 log_buffer는 \n\0로 끝남
//             // strncat(log_print_buffer, log_buffer, strlen(log_buffer));
//             // log_buffer의 내용을 log_print_buffer로 복사
//             size_t log_buffer_len = strlen(log_buffer);
//             memcpy(log_print_ptr, log_buffer, log_buffer_len);

//             // log_ptr 이동 및 \0 추가
//             log_ptr += log_buffer_len;

//             // printf(log_print_buffer);
//             log_index++;
//             // printf("%d\n", log_index);

//             // 100개가 모이면 파일에 출력
//             if (log_index == MAX_LINES) {
//                 // fprintf(logfile_whole, log_print_buffer);

//                 *log_ptr = '\0';  // 문자열 종료
//                 fwrite(log_print_buffer, sizeof(char), strlen(log_print_buffer), logfile_whole);

//                 memset(log_print_buffer, 0, sizeof(log_print_buffer));  // 버퍼의 모든 바이트를 0으로 설정ㄴ
//                 log_index = 0;  // 버퍼 초기화
//                 // 끝 시간 측정
//                 auto log_end_time = std::chrono::steady_clock::now();
//                 // 실행 시간 계산
//                 auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(log_end_time - log_start_time).count();
//                 // printf( "process_can_msg 실행 시간: %.6fms\n", (elapsed_time/ 1000.0 )/MAX_LINES);
//                 printf( "process_can_msg 실행 시간: %.6fms\n", (elapsed_time/ 1000.0 )/MAX_LINES);
//                 log_start_time=std::chrono::steady_clock::now();   
//             }
//             //초기화들
            memset(log_buffer, 0, sizeof(log_buffer));  // 버퍼의 모든 바이트를 0으로 설정ㄴ
            log_ptr=log_buffer;

            stats.last_timestamp = dequeuedMsg.timestamp;
            memcpy(stats.last_data, dequeuedMsg.data, sizeof(stats.last_data));

	        fflush(logfile_whole);
            // 끝 시간 측정
            auto log_end_time = std::chrono::steady_clock::now();
            // 실행 시간 계산
            auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(log_end_time - log_start_time).count();
            printf( "process_can_msg 실행 시간: %.6fms\n", (elapsed_time/ 1000.0 ));
            log_start_time=std::chrono::steady_clock::now(); 

            lock.lock();            
        }
    }
    printf("%d\n",log_index);
    // 100개가 모이면 파일에 출력
    if (log_index > 0) {
        fprintf(logfile_whole, log_print_buffer);
    }
}

// 저장된 CAN 메시지 출력 (디버그용)
void debugging_dequeuedMsg(EnqueuedCANMsg* dequeuedMsg){
    printf("Timestamp: %.6f\n", dequeuedMsg->timestamp);
    printf("CAN ID:%03X\n",dequeuedMsg->can_id);
    printf("DLC: %d\n", dequeuedMsg->DLC);
    printf("Data: ");
    for (int i = 0; i < dequeuedMsg->DLC; i++) {
        printf("%02X ", dequeuedMsg->data[i]);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    printAsciiArt();

    int s;
    char *log_filename=argv[1];
    struct sockaddr_can addr;
    struct ifreq ifr;
    EnqueuedCANMsg can_msg;  // 수신된 CAN 메시지를 저장할 구조체
    
    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket creation error");
        return 1;
    }

    // 인터페이스 이름을 설정 (vcan0 사용)
    strcpy(ifr.ifr_name, INTERFACE_NAME);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("IOCTL error");
        return 1;
    }

    // 소켓 바인딩
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind error");
        return 1;
    }
    
    // 타이머 확인을 위한 스레드 생성 및 분리
    std::thread timerThread(timerCheckThread);
    timerThread.detach();  // 타이머 확인 스레드를 메인 스레드와 분리하여 백그라운드에서 실행

    q_init(&canMsgQueue, sizeof(EnqueuedCANMsg), CAN_MSSG_QUEUE_SIZE, IMPLEMENTATION, false);
    
    int rows, cols;
    if (getCursorPosition(rows, cols) == 0) {
        std::cout << "현재 커서 위치: " << rows << "행, " << cols << "열" << std::endl;
	current_row = rows+2;
    }
    printoneiArt();
    std::cout << "\033[" << rows << ";" << cols << "H";

    std::thread producerThread(receive_can_frame, s, &can_msg);
    std::thread consumerThread(process_can_msg, log_filename);
    
    // Wait for the threads to finish before exiting the program
    producerThread.join();
    consumerThread.join();

    // 커서를 마지막 줄 다음 줄로 이동
    std::cout << "\033[3B" << std::flush;
    
    close(s);
    return 0;
}
