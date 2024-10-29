#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "header.h"
#include "periodic.h"
#include "all_attack_detection.h"

#define CAN_MSSG_QUEUE_SIZE 100 //큐에 담을수 있는 데이터 사이즈
#define IMPLEMENTATION FIFO //선입선출로 큐를 초기화할때 사용

std::mutex queueMutex;
std::condition_variable queueCondVar;
bool done = false;
double start_time=0;
struct timeval tv;

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
	    
	    fprintf(logfile_whole, "can0 %03X#", dequeuedMsg.can_id);
	    for (int i = 0; i < dequeuedMsg.DLC; i++) {
                fprintf(logfile_whole, "%02X", dequeuedMsg.data[i]);
            }

            CANStats& stats = can_stats[dequeuedMsg.can_id];
            if(dequeuedMsg.timestamp - start_time <= 40){
                fprintf(logfile_whole, " 0\n");
                calc_periodic(dequeuedMsg.can_id, dequeuedMsg.timestamp);
            }
            else if(check){
                fprintf(logfile_whole, " 0\n");
                MIN_CAN_ID = get_lowest_can_id();
                check = false;
            }
            else if (filtering_process(&dequeuedMsg)){
                stats.event_count = -1;
          
                stats.prev_timediff = 0;
                fprintf(logfile_whole, " 1\n");
		        printf("Malicious packet! count: %d\n", mal_count++);
            }
            else {
		        fprintf(logfile_whole, " 0\n");
            }

            stats.prev_timediff = dequeuedMsg.timestamp - stats.last_timestamp;
            stats.last_timestamp = dequeuedMsg.timestamp;
            memcpy(stats.last_data, dequeuedMsg.data, sizeof(stats.last_data));
            lock.lock();
	        fflush(logfile_whole);
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

int main() {
    int s;
    char log_filename[100];
    struct sockaddr_can addr;
    struct ifreq ifr;
    EnqueuedCANMsg can_msg;  // 수신된 CAN 메시지를 저장할 구조체
    std::cout << "Enter the name of the log file (e.g., ../dataset/whole_replay.log): ";
    std::cin.getline(log_filename, sizeof(log_filename));

    std::string filename;
    std::cout<<"input dbc file name(continue except dbc file, input -1): ";
    std::getline(std::cin, filename);
    if(filename != "-1") {
	    read_dbc(filename);
    }

    // 소켓 생성
    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket creation error");
        return 1;
    }

    // 인터페이스 이름을 설정 (vcan0 사용)
    strcpy(ifr.ifr_name, "vcan0");
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

    q_init(&canMsgQueue, sizeof(EnqueuedCANMsg), CAN_MSSG_QUEUE_SIZE, IMPLEMENTATION, false);
    
    printf("Starting Periodic Calculation 10 seconds\n");
    
    std::thread producerThread(receive_can_frame, s, &can_msg);
    std::thread consumerThread(process_can_msg, log_filename);
    
    // Wait for the threads to finish before exiting the program
    producerThread.join();
    consumerThread.join();
    

    close(s);
    return 0;
}
