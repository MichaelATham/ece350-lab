/*
 * k_task.h
 *
 *  Created on: Jan 5, 2024
 *      Author: nexususer
 *
 *      NOTE: any C functions you write must go into a corresponding c file that you create in the Core->Src folder
 */

#ifndef INC_K_TASK_H_
#define INC_K_TASK_H_

#include "common.h"

#define SHPR2 *(uint32_t*)0xE000ED1C //for setting SVC priority, bits 31-24
#define SHPR3 *(uint32_t*)0xE000ED20 //PendSV is bits 23-16

typedef unsigned int U32;
typedef unsigned short U16;
typedef char U8;
typedef unsigned int task_t;

typedef struct task_control_block
{
	void (*ptask)(void *args); // entry address
	U32 stack_high;			   // starting address of stack (high address)
	task_t tid;				   // task ID
	U8 state;				   // task's state
	U16 stack_size;			   // stack size. Must be a multiple of 8
	// your own fields at the end
	U32 *stack_ptr; // current address of stack ptr
	U32 pc;			// program counter
	int deadline; 	// Milliseconds before task must be switched out
	int sleepingTime; // Time in MS that the task needs to sleep for
	int originalDeadline; // non volatile deadline
	U32 *mem_ptr;	// Starting address of the allocated memory. We can't change this since we need to call it in dealloc
} TCB;

typedef struct kernel
{
	// Queue for the Tasks in waiting list
	QUEUE queue;

	// Current Address of Running Task
	int currTaskId;

	// Array of Task Control Blocks
	TCB tcbArray[MAX_TASKS];

	// Available Memory in Stack
	U32 availMemory;
} KNL;

// Kernel API Definitions
void osKernelInit(void);

int osKernelStart(void);

int osCreateTask(TCB *task);

void osYield(void);

int osTaskInfo(task_t TID, TCB *task_copy);

task_t osGetTID(void);

int osTaskExit(void);

int osCheckTime(U32 counter);

void edfScheduler();

#endif /* INC_K_TASK_H_ */
