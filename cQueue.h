#ifndef CQUEUE_H
#define CQUEUE_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#define QUEUE_INITIALIZED 0x5AA5
#define q_init_def(q, sz) q_init(q, sz, 20, FIFO, false)

#define q_pull q_pop
#define q_nbRecs q_getCount
#define q_clean q_flush

typedef enum enumQueueType {
	FIFO = 0,
	LIFO = 1
} QueueType;

typedef struct Queue_t {
	QueueType impl;
	bool      ovw;
	uint16_t  rec_nb;
	uint16_t  rec_sz;
	uint32_t  queue_sz;
	uint8_t* queue;

	uint16_t  in;
	uint16_t  out;
	uint16_t  cnt;
	uint16_t  init;
} Queue_t;

void q_flush(Queue_t* const q);
inline bool q_isFull(const Queue_t* const q) {
	return (q->cnt == q->rec_nb) ? true : false; }

void* q_init(Queue_t* const q, const uint16_t size_rec, const uint16_t nb_recs, const QueueType type, const bool overwrite);
void q_kill(Queue_t* const q);
inline bool q_isInitialized(const Queue_t* const q) {
	return (q->init == QUEUE_INITIALIZED) ? true : false;
}
inline bool q_isEmpty(const Queue_t* const q) {
	return (!q->cnt) ? true : false;
}
inline uint32_t q_sizeof(const Queue_t* const q) {
	return q->queue_sz;
}
inline uint16_t q_getCount(const Queue_t* const q) {
	return q->cnt;
}
inline uint16_t q_getRemainingCount(const Queue_t* const q) {
	return q->rec_nb - q->cnt;
}
bool q_push(Queue_t* const q, const void* const record);
bool q_pop(Queue_t* const q, void* const record);
bool q_peek(const Queue_t* const q, void* const record);
bool q_drop(Queue_t* const q);
bool q_peekIdx(const Queue_t* const q, void* const record, const uint16_t idx);

inline bool q_peekPrevious(const Queue_t* const q, void* const record) {
	const uint16_t idx = q_getCount(q) - 1;
	return q_peekIdx(q, record, idx);
}

uint16_t q_getSize(const Queue_t* const q);

#endif
