#include <stdio.h>
#include "cQueue.h"

#define CAN_MSSG_QUEUE_SIZE 100
#define IMPLEMENTATION FIFO
Queue_t canMsgQueue;

typedef struct qCANMsg {
    uint8_t data[8];
    //....
}EnqueuedCANMsg;

void receiveCANMessage(const uint8_t* data) {
    EnqueuedCANMsg msg;
    // 데이터를 msg 구조체에 복사
    memcpy(msg.data, data, sizeof(msg.data));

    // 큐에 메시지를 삽입
    if (!q_push(&canMsgQueue, &msg)) {
        printf("Queue is full.\n");
    }
}

void main() {
    q_init(&canMsgQueue, sizeof(EnqueuedCANMsg), CAN_MSSG_QUEUE_SIZE, IMPLEMENTATION, false);

    // 가상 수신 데이터DDDd
    uint8_t receivedData1[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    uint8_t receivedData2[8] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18 };

    // 데이터를 수신하여 큐에 삽입
    receiveCANMessage(receivedData1);
    receiveCANMessage(receivedData2);

    // 나중에 데이터를 큐에서 꺼내서 처리
    EnqueuedCANMsg dequeuedMsg;
    if (q_pop(&canMsgQueue, &dequeuedMsg)) {
        printf("Dequeued Message: ");
        for (int i = 0; i < 8; i++) {
            printf("%02X ", dequeuedMsg.data[i]);
        }
        printf("\n");
    }

    return 0;
}