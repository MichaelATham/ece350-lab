/*
 * queue.c
 *
 *  Created on: May 23, 2024
 *      Author: jinhakim
 */
#include "queue.h"

int size(QUEUE q)
{
	return (16 - q.front + q.rear);
}

int isEmpty(QUEUE q)
{
	return (size(q) == 16);
}

int front(QUEUE q)
{
	if (isEmpty(q))
	{
		return -1;
	}

	return q.arr[q.front];
}

task_t dequeue(QUEUE *q)
{
	if (isEmpty(*q))
	{
		return -1;
	}

	task_t res = q->arr[q->front];

	q->arr[q->front] = 0;

	q->front = (q->front + 1) % 16;

	return res;
}

int enqueue(QUEUE *q, task_t addr)
{

	if (size(*q) == 15)
	{
		return -1;
	}

	q->arr[q->rear] = addr;
	q->rear = (q->rear + 1) % 16;

	return 0;
}
