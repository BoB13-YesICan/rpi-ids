#include "header.h"

#define INTERFACE_NAME "vcan0"

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
    int mal_count = 0;
    FILE *logfile_whole = fopen(log_filename, "w");
    bool check = true;
    while(!done){
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCondVar.wait(lock, []{return !q_isEmpty(&canMsgQueue)|| done; });
        while ((!q_isEmpty(&canMsgQueue))){
            EnqueuedCANMsg dequeuedMsg;
            q_pop(&canMsgQueue, &dequeuedMsg);

            lock.unlock();

            fprintf(logfile_whole, "%.6f can0 %03X#", dequeuedMsg.timestamp, dequeuedMsg.can_id);
            for (int i = 0; i < dequeuedMsg.DLC; i++) {
                fprintf(logfile_whole, "%02X", dequeuedMsg.data[i]);
            }

            CANStats& stats = can_stats[dequeuedMsg.can_id];
          
            if(dequeuedMsg.timestamp - start_time <= 40&&stats.count < 201){
                fprintf(logfile_whole, " 0\n");
                calc_periodic(dequeuedMsg.can_id, dequeuedMsg.timestamp);
                
                #ifdef SET_DBC_CHECK
                calc_similarity(dequeuedMsg.can_id, dequeuedMsg.data, dequeuedMsg.DLC, stats.valid_last_data, stats.similarity_percent, stats.count);
                #endif
            
            } else if(susp[dequeuedMsg.can_id]){
                susp[dequeuedMsg.can_id] = 0;
                stats.event_count = -1;
                stats.prev_timediff = 0;
                printf("Suspended packet! count: %d\n", mal_count++);
                fprintf(logfile_whole, " 1\n");
            } else if(check){
	            fprintf(logfile_whole, " 0\n");
                MIN_CAN_ID = get_lowest_can_id();
                check = false;
            }
	          else if (filtering_process(&dequeuedMsg)){
                stats.event_count = -1;
                stats.prev_timediff = 0;
                fprintf(logfile_whole, " 1\n");
                printf("Malicious packet! count: %d\n", mal_count++);
            } else{
                onCanMessageReceived(dequeuedMsg.can_id);
                fprintf(logfile_whole, " 0\n");
            }
            stats.last_timestamp = dequeuedMsg.timestamp;
            memcpy(stats.last_data, dequeuedMsg.data, sizeof(stats.last_data));
	        fflush(logfile_whole);

            lock.lock();
        }
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
    int s;
    //char log_filename[100];
    char *log_filename=argv[1];
    struct sockaddr_can addr;
    struct ifreq ifr;
    EnqueuedCANMsg can_msg;  // 수신된 CAN 메시지를 저장할 구조체
    //std::cout << "Enter the name of the log file (e.g., ../dataset/whole_replay.log): ";
    //std::cin.getline(log_filename, sizeof(log_filename));

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
        
    std::thread producerThread(receive_can_frame, s, &can_msg);
    std::thread consumerThread(process_can_msg, log_filename);
    
    // Wait for the threads to finish before exiting the program
    producerThread.join();
    consumerThread.join();
    

    close(s);
    return 0;
}
