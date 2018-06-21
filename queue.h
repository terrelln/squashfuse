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

#ifndef SQFS_QUEUE_H
#define SQFS_QUEUE_H

#include "common.h"

#include <pthread.h>


/* Simple thread-safe queue with limited capacity. */

typedef void (*sqfs_queue_free_t)(void *);

typedef struct {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	char *items;
	size_t pop_index;
	size_t push_index;
	size_t value_size;
	size_t size;
	sqfs_queue_free_t freer;
	bool finished;
} sqfs_queue;

/* Initializes the queue with a fixed size. */
sqfs_err sqfs_queue_init(sqfs_queue *q, size_t vsize, size_t size,
		sqfs_queue_free_t freer);

/* Destroys the queue. This function is not thread-safe, so the queue must not
 * be in use. Call sqfs_queue_finish() to start the queue shutdown.
 */
void sqfs_queue_destroy(sqfs_queue *q);

/* Signals that the queue is finished. All succeeding pushes will fail, and pop
 * will rturn false when the queue is empty.
 */
void sqfs_queue_finish(sqfs_queue *q);

/* Attempts to push an element onto the queue, and returns false if the queue
 * is full or sqfs_queue_finish() has been called.
 * Returns true on success, in which case the queue takes ownership of vin.
 */
bool sqfs_queue_try_push(sqfs_queue *q, void *vin);

/* Pop and element from the queue and block if it is empty.
 * Returns true on success, and the queue moves ownership into vout.
 * Returns false after sqfs_queue_finish() has been called AND the queue is
 * empty.
 */
bool sqfs_queue_pop(sqfs_queue *q, void *vout);

typedef bool (*sqfs_queue_find_t)(void const*, void const*);
/* Returns true if pred returns true for any element in the queue.
 * pred() should take opaque as its first argument, and a queue entry as its
 * second argument.
 * NOTE: The queue is locked for the entire function, so pred should be fast and
 * the queue should be small.
 */
bool sqfs_queue_find(sqfs_queue *q, void const *opaque, sqfs_queue_find_t pred);

#endif /* SQFS_QUEUE_H */
