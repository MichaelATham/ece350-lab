/*
 * queue.h
 *
 *  Created on: May 23, 2024
 *      Author: jinhakim
 */

#ifndef INC_QUEUE_H_
#define INC_QUEUE_H_

typedef unsigned int task_t;

typedef struct queue
{
	// Queue stores task TIDs
	task_t arr[16];
	int front;
	int rear;
} QUEUE;

int size(QUEUE);

int isEmpty(QUEUE);

int front(QUEUE);

task_t dequeue(QUEUE *);

int enqueue(QUEUE *, task_t);

#endif /* INC_QUEUE_H_ */
