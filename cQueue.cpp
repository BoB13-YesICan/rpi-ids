#include <string.h>
#include <stdlib.h>

#include "cQueue.h"

static inline void inc_idx(uint16_t* const pIdx, const uint16_t end, const uint16_t start)
{
	if (*pIdx < end - 1) { (*pIdx)++; }
	else { *pIdx = start; }
}

static inline void dec_idx(uint16_t* const pIdx, const uint16_t end, const uint16_t start)
{
	if (*pIdx > start) { (*pIdx)--; }
	else { *pIdx = end - 1; }
}


void* q_init(Queue_t* const q, const uint16_t size_rec, const uint16_t nb_recs, const QueueType type, const bool overwrite)
{
	const uint32_t size = nb_recs * size_rec;

	q->rec_nb = nb_recs;
	q->rec_sz = size_rec;
	q->impl = type;
	q->ovw = overwrite;

	q_kill(q);	
	q->queue = (uint8_t*)malloc(size);

	if (q->queue == NULL) { q->queue_sz = 0; return 0; }
	else { q->queue_sz = size; }

	q->init = QUEUE_INITIALIZED;
	q_flush(q);

	return q->queue;	
}

void q_kill(Queue_t* const q)
{
	if (q->init == QUEUE_INITIALIZED) { free(q->queue); }	
	q->init = 0;
}


void q_flush(Queue_t* const q)
{
	q->in = 0;
	q->out = 0;
	q->cnt = 0;
}


bool q_push(Queue_t* const q, const void* const record)
{
	if ((!q->ovw) && q_isFull(q)) { return false; }

	uint8_t* const pStart = q->queue + (q->rec_sz * q->in);
	memcpy(pStart, record, q->rec_sz);

	inc_idx(&q->in, q->rec_nb, 0);

	if (!q_isFull(q)) { q->cnt++; }	
	else if (q->ovw)					
	{
		if (q->impl == FIFO) { inc_idx(&q->out, q->rec_nb, 0); }
	}

	return true;
}

bool q_pop(Queue_t* const q, void* const record)
{
	const uint8_t* pStart;

	if (q_isEmpty(q)) { return false; }	

	if (q->impl == FIFO)
	{
		pStart = q->queue + (q->rec_sz * q->out);
		inc_idx(&q->out, q->rec_nb, 0);
	}
	else if (q->impl == LIFO)
	{
		dec_idx(&q->in, q->rec_nb, 0);
		pStart = q->queue + (q->rec_sz * q->in);
	}
	else { return false; }

	memcpy(record, pStart, q->rec_sz);
	q->cnt--;	
	return true;
}

bool q_peek(const Queue_t* const q, void* const record)
{
	const uint8_t* pStart;

	if (q_isEmpty(q)) { return false; }	

	if (q->impl == FIFO)
	{
		pStart = q->queue + (q->rec_sz * q->out);
	}
	else if (q->impl == LIFO)
	{
		uint16_t rec = q->in;	
		dec_idx(&rec, q->rec_nb, 0);
		pStart = q->queue + (q->rec_sz * rec);
	}
	else { return false; }

	memcpy(record, pStart, q->rec_sz);
	return true;
}

bool q_drop(Queue_t* const q)
{
	if (q_isEmpty(q)) { return false; }

	if (q->impl == FIFO) { inc_idx(&q->out, q->rec_nb, 0); }
	else if (q->impl == LIFO) { dec_idx(&q->in, q->rec_nb, 0); }
	else { return false; }

	q->cnt--;
	return true;
}

bool q_peekIdx(const Queue_t* const q, void* const record, const uint16_t idx)
{
	const uint8_t* pStart;

	if (idx + 1 > q_getCount(q)) { return false; }	

	if (q->impl == FIFO)
	{
		pStart = q->queue + (q->rec_sz * ((q->out + idx) % q->rec_nb));
	}
	else if (q->impl == LIFO)
	{
		pStart = q->queue + (q->rec_sz * idx);
	}
	else { return false; }

	memcpy(record, pStart, q->rec_sz);
	return true;
}
uint16_t q_getSize(const Queue_t* const q)
{
    return q->cnt; // Return the current count of elements in the queue
}
