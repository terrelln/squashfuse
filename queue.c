/*
 * Copyright (c) 2014 Dave Vasilevsky <dave@vasilevsky.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "queue.h"

#include <stdlib.h>
#include <string.h>

static void *sqfs_queue_at(sqfs_queue *q, size_t index) {
	return q->items + q->value_size * index;
}

sqfs_err sqfs_queue_init(sqfs_queue *q, size_t vsize, size_t size,
		sqfs_queue_free_t freer) {
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);
	q->pop_index = 0;
	q->push_index = 0;
	q->value_size = vsize;
	/* Allocate space for size + 1 elements because we need to distinguish empty
	 * vs full. pop_index == push_index means empty. push_index = pop_index - 1
	 * means full.
	 */
	q->size = size + 1;
	q->finished = false;
	q->freer = freer;
	q->items = (char *)malloc(q->value_size * q->size);
	if (!q->items)
		return SQFS_ERR;
	return SQFS_OK;
}

void sqfs_queue_finish(sqfs_queue *q) {
	pthread_mutex_lock(&q->mutex);
	q->finished = true;
	pthread_cond_broadcast(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

static size_t sqfs_queue_next(sqfs_queue *q, size_t index) {
	size_t const next = index + 1;
	if (next == q->size)
		return 0;
	return next;
}

void sqfs_queue_destroy(sqfs_queue *q) {
	if (q == NULL)
		return;

	pthread_mutex_destroy(&q->mutex);
	pthread_cond_destroy(&q->cond);

	while (q->pop_index != q->push_index) {
		q->freer(sqfs_queue_at(q, q->pop_index));
		q->pop_index = sqfs_queue_next(q, q->pop_index);
	}

	free(q->items);
}

bool sqfs_queue_try_push(sqfs_queue *q, void *vin) {
	bool pushed = false;
	size_t next;

	pthread_mutex_lock(&q->mutex);
	next = sqfs_queue_next(q, q->push_index);
	/* Queue is full if the next push index is pop_index */
	if (next != q->pop_index && !q->finished) {
		memcpy(sqfs_queue_at(q, q->push_index), vin, q->value_size);
		q->push_index = next;
		pushed = true;
		/* Wake a waiting popper (if any) */
		pthread_cond_signal(&q->cond);
	}
	pthread_mutex_unlock(&q->mutex);

	return pushed;
}

bool sqfs_queue_pop(sqfs_queue *q, void *vout) {
	bool popped = false;

	pthread_mutex_lock(&q->mutex);
	while (q->pop_index == q->push_index && !q->finished) {
		pthread_cond_wait(&q->cond, &q->mutex);
	}
	/* We will drain the queue even if finished */
	if (q->pop_index != q->push_index) {
		memcpy(vout, sqfs_queue_at(q, q->pop_index), q->value_size);
		q->pop_index = sqfs_queue_next(q, q->pop_index);
		popped = true;
	}
	pthread_mutex_unlock(&q->mutex);
	return popped;
}

static size_t sqfs_queue_prev(sqfs_queue *q, size_t index) {
	if (index == 0)
		return q->size - 1;
	return index - 1;
}

bool sqfs_queue_find(sqfs_queue *q, void const *opaque,
		sqfs_queue_find_t pred) {
	bool found = false;
	size_t index;
	/* Iterate backwards to catch any temporal correlations. */
	pthread_mutex_lock(&q->mutex);
	index = q->push_index;
	while (index != q->pop_index) {
		index = sqfs_queue_prev(q, index);
		if (pred(opaque, sqfs_queue_at(q, index))) {
			found = true;
			break;
		}
	}
	pthread_mutex_unlock(&q->mutex);
	return found;
}
