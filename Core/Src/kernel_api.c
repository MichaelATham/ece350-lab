/*
 * kernel_api.c
 *
 *  Created on: May 23, 2024
 *      Author: jinhakim
 */
#include "k_mem.h"
#include "common.h"
#include "k_task.h"
#include <stdio.h>


static KNL kernel;
static int isInitialized = 0;
static unsigned int * currSP;
static unsigned int sleepingTasks = 0;
int totalTasks = 0;

int nullTaskCreated = 0;

U32 isRunning = 0;

static task_t nextTask = 0; 

void nullTask(void *)
{
	while(1){

	}
}

int get_isInitialized(void) {
    return isInitialized;
}

// Initialize Kernel with global-level data structures
void osKernelInit(void)
{

	SHPR3 |= 0xFFU << 24; // Set the priority of SysTick to be the weakest
	SHPR3 |= 0xFEU << 16; // shift the constant 0xFE 16 bits to set PendSV priority
	SHPR2 |= 0xFDU << 24; // set the priority of SVC higher than PendSV

	// If Kernel is already initialized, don't initialize again!!
	if (isInitialized)
	{
		return;
	}

	// Initialize TCB arr with initial values
	for (int i = 0; i < MAX_TASKS; i++)
	{
		// Initializing Null Task
		if (i == 0)
		{
			kernel.tcbArray[i].tid = TID_NULL;
			kernel.tcbArray[i].state = READY; // Set initial state
			kernel.tcbArray[i].stack_size = 0;
			kernel.tcbArray[i].stack_high = 0;
			kernel.tcbArray[i].ptask = &nullTask;
			kernel.tcbArray[i].pc = &nullTask;
			kernel.tcbArray[i].deadline = 5; 	// 5ms
			kernel.tcbArray[i].originalDeadline = 5;
		}
		else
		{
			kernel.tcbArray[i].tid = i;
			kernel.tcbArray[i].state = DORMANT; // Set initial state
			kernel.tcbArray[i].stack_size = 0;
			kernel.tcbArray[i].stack_high = 0;
			kernel.tcbArray[i].deadline = 5; 	// 5ms
			kernel.tcbArray[i].originalDeadline = 5;

		}
	}

	kernel.currTaskId = -1;
	// Potentially also subtract the null tasks size
	kernel.availMemory = MAX_HEAP_SIZE;

	isInitialized = 1;

	k_mem_init();

	// INIT NULL TASK
	TCB nTask;
	nTask.stack_size = STACK_SIZE;
	nTask.ptask = &nullTask;
	nTask.tid = 0;
	osCreateTask(&nTask);

}

// Run first task, initialize scheduling algorithm
int osKernelStart()
{
	if (!isInitialized || isRunning)
	{
		return RTX_ERR;
	}

	for(int i = 0; i < MAX_TASKS; i++) {
		kernel.tcbArray[i].deadline = kernel.tcbArray[i].originalDeadline;
	}

	__disable_irq();
	isRunning = 1;
	edfScheduler();

	return RTX_OK;
}

int osCreateTask(TCB *task)
{
	// Return error if task stack size less than min stack size
	if (task == NULL || task->stack_size < STACK_SIZE || task->ptask == NULL)
	{
		return RTX_ERR;
	}

	// Main stack pointer initial value
	U32 *MSP_INIT_VAL = *(U32 **)0x0;

	// Check if memory is available, if not return error
	if (task->stack_size > kernel.availMemory)
	{
		return RTX_ERR;
	}

	// Finding a dormant task in the tcbArray
	int taskIndex = -1;

	// Max U16 value, we want to find the minimum stack size that accomodates our task
	U16 stack_size = 65535;

	//disable interrupts so that the task creation process doesn't get preempted in the middle and causes data corruption
	__disable_irq();

	// dynamically initialize nulltask so that we can context switch to it
	if(task->tid == 0 && !nullTaskCreated) {

		taskIndex = 0;

		kernel.tcbArray[taskIndex].ptask = task->ptask;
		kernel.tcbArray[taskIndex].pc = task->ptask; // Set the program counter to the task entry point
		kernel.tcbArray[taskIndex].state = READY;
		kernel.tcbArray[taskIndex].tid = taskIndex;
		kernel.tcbArray[taskIndex].deadline = 5; // 5ms default
		kernel.tcbArray[taskIndex].originalDeadline = 5;
		kernel.tcbArray[taskIndex].stack_size = task->stack_size;


		// get pointer from alloc so we can access the data block struct to modify the owner tid
		void* newTaskPointer = k_mem_alloc(kernel.tcbArray[taskIndex].stack_size);

		if(newTaskPointer == NULL) {
			return RTX_ERR;
		}
		uintptr_t dum_adress = (uintptr_t)newTaskPointer - sizeof(data_Block);
		data_Block* buddy_block = (data_Block*)dum_adress;
		buddy_block->owner_tid = taskIndex;

		kernel.tcbArray[taskIndex].mem_ptr = newTaskPointer;

		// Calculating stack pointer
		kernel.tcbArray[taskIndex].stack_ptr = kernel.tcbArray[taskIndex].mem_ptr;

		// Update available memory
		kernel.availMemory -= kernel.tcbArray[taskIndex].stack_size;


		*(--kernel.tcbArray[taskIndex].stack_ptr) = 1 << 24;							   // xPSR, setting chip to thumb mode
		*(--kernel.tcbArray[taskIndex].stack_ptr) = (U32)kernel.tcbArray[taskIndex].ptask;

		for (int i = 0; i < 14; i++)
		{
			*(--kernel.tcbArray[taskIndex].stack_ptr) = 0xA; // An arbitrary number
		}

		// so we don't initialize the nulltask more than once
		nullTaskCreated = 1;

		return RTX_OK;

	}

	for (int i = 1; i < MAX_TASKS; i++)
	{
		if (kernel.tcbArray[i].state == DORMANT && kernel.tcbArray[i].stack_size == 0)
		{
			taskIndex = i;
			break;
		}

		// Finding a dormant stack with a higher or equal stack_size value, that is the minimum
		// This is to prevent tasks from not being able to run
		// i.e. one task is on a stack that has 5000 bytes, but only requires 512. Another task requires 4000 bytes but can't find a suitable stack
		else if (kernel.tcbArray[i].state == DORMANT && kernel.tcbArray[i].stack_size >= task->stack_size && kernel.tcbArray[i].stack_size <= stack_size)
		{
			taskIndex = i;
			stack_size = kernel.tcbArray[i].stack_size;
			break;
		}
	}

	// Max amount of tasks scheduled
	if (taskIndex == -1)
	{
		return RTX_ERR;
	}

	// Assigning tid to task (REQUIRED)
	task->tid = taskIndex;

	// Creating a task in a never before allocated stack
	// These values stay the same otherwise, as we don't do any memory management apart from the first task created per stack
	if (stack_size == 65535)
	{
		// Calculate total stack size taken by other tasks
		U32 total_stack_size_taken = 0;
		for (int i = 0; i < taskIndex; i++)
		{
			if (kernel.tcbArray[i].state != DORMANT)
			{
				total_stack_size_taken += kernel.tcbArray[i].stack_size;
			}
		}

		kernel.tcbArray[taskIndex].stack_high = MSP_INIT_VAL - MAX_HEAP_SIZE - total_stack_size_taken;
		kernel.tcbArray[taskIndex].stack_size = task->stack_size;
	}

	// Setting task values to tcb array
	kernel.tcbArray[taskIndex].ptask = task->ptask;
	kernel.tcbArray[taskIndex].pc = task->ptask; // Set the program counter to the task entry point
	kernel.tcbArray[taskIndex].state = READY;
	kernel.tcbArray[taskIndex].tid = taskIndex;
	kernel.tcbArray[taskIndex].deadline = 5; // 5ms default
	kernel.tcbArray[taskIndex].originalDeadline = 5;


	// get pointer from alloc so we can access the data block struct to modify the owner tid
	void* newTaskPointer = k_mem_alloc(kernel.tcbArray[taskIndex].stack_size);

	if (newTaskPointer == NULL) {
		return RTX_ERR;
	}

	uintptr_t dum_adress = (uintptr_t)newTaskPointer - sizeof(data_Block);
	data_Block* buddy_block = (data_Block*)dum_adress;
	buddy_block->owner_tid = taskIndex;
	kernel.tcbArray[taskIndex].mem_ptr = newTaskPointer;

	// adding offset of stack_size so that we can do modifications safely without modifying metadeta
	void* tmp = (uintptr_t) newTaskPointer + (uintptr_t) kernel.tcbArray[taskIndex].stack_size;

	// Setting stack pointer to temporary pointer calculated above
	kernel.tcbArray[taskIndex].stack_ptr = tmp;

	// Update available memory
	kernel.availMemory -= kernel.tcbArray[taskIndex].stack_size;


	*(--kernel.tcbArray[taskIndex].stack_ptr) = 1 << 24;							   // xPSR, setting chip to thumb mode
	*(--kernel.tcbArray[taskIndex].stack_ptr) = (U32)kernel.tcbArray[taskIndex].ptask;

	for (int i = 0; i < 14; i++)
	{
		*(--kernel.tcbArray[taskIndex].stack_ptr) = 0xA; // An arbitrary number
	}


	totalTasks++;

	// Special case when the created task pre-empts the task that created it
	if(kernel.tcbArray[taskIndex].deadline < kernel.tcbArray[kernel.currTaskId].deadline 
		|| ((kernel.tcbArray[taskIndex].deadline == kernel.tcbArray[kernel.currTaskId].deadline) && (taskIndex < kernel.currTaskId))) {
		nextTask = taskIndex;
		if(isRunning) {
			nextTask = taskIndex;
			__enable_irq();
			SCB->ICSR |= 1<<28; //control register bit for a PendSV interrupt
			__asm("isb");
		}
	}
	
	//enable interrupts after creating the task so that there is no data corruption or preemption happening during the process of creating a task
	__enable_irq();

	return RTX_OK;
}

int osTaskInfo(task_t TID, TCB *task_copy)
{
	// Return error if TID is out of bounds
	if (TID < 0 || TID >= MAX_TASKS)
	{
		return RTX_ERR;
	}

	// Return false if TID has dormant state
	if (kernel.tcbArray[TID].state == DORMANT)
	{
		return RTX_ERR;
	}

	// Copy all values from Task
	task_copy->ptask = kernel.tcbArray[TID].ptask;
	task_copy->stack_high = kernel.tcbArray[TID].stack_high;
	task_copy->tid = kernel.tcbArray[TID].tid;
	task_copy->state = kernel.tcbArray[TID].state;
	task_copy->stack_size = kernel.tcbArray[TID].stack_size;
	task_copy->stack_ptr = kernel.tcbArray[TID].stack_ptr;
	task_copy->pc = kernel.tcbArray[TID].pc;
	task_copy->deadline = kernel.tcbArray[TID].deadline;
	task_copy->originalDeadline = kernel.tcbArray[TID].originalDeadline;
	task_copy->mem_ptr = kernel.tcbArray[TID].mem_ptr;

	return RTX_OK;
}

// Retrieve TID from stack?
task_t osGetTID()
{
	if (nextTask == 0) { return 0; }
	return kernel.currTaskId;
}

int osTaskExit(void) {
	// Return rtx error if not called by running task
	if(kernel.tcbArray[kernel.currTaskId].state == DORMANT){
		return RTX_ERR;
	}

	//disable interrupts so that the task exit process doesn't get preempted in the middle and causes data corruption
	__disable_irq();

	// save current running task's context set task status back to ready from running
	kernel.tcbArray[kernel.currTaskId].state = DORMANT;

	// Update Available Memory
	kernel.availMemory += kernel.tcbArray[kernel.currTaskId].stack_size;

	k_mem_dealloc(kernel.tcbArray[kernel.currTaskId].mem_ptr);

	totalTasks--;

	edfScheduler();

	__enable_irq();
	// Initiating a system call

	return RTX_OK;
}

void osYield(void) {

	// if calling osYield from main then return an error
	if(kernel.currTaskId == -1) {
		return RTX_ERR;
	}

	__disable_irq();
	// reset the task's remaining time back to the deadline
	kernel.tcbArray[kernel.currTaskId].deadline = kernel.tcbArray[kernel.currTaskId].originalDeadline;

	edfScheduler();

	return;

}

// SVC Handler Main sets the PSP to the next task
void SVC_Handler_Main(unsigned int* svc_args)
{

	// Not context switching to another Task. Likely Main to T1
	if(kernel.currTaskId != -1) {
		kernel.tcbArray[kernel.currTaskId].stack_ptr = __get_PSP();
	}

	// save current running task's context set task status back to ready from running
	if(kernel.currTaskId != -1 && kernel.tcbArray[kernel.currTaskId].state != SLEEPING && kernel.tcbArray[kernel.currTaskId].state != DORMANT) {
		kernel.tcbArray[kernel.currTaskId].state = READY;
	}

	// scheduler is run and next task is selected (from waiting kernel queue)
	kernel.currTaskId = nextTask;

	// set task status to running and resume execution
	kernel.tcbArray[nextTask].state = RUNNING;

	// Setting the PSP
	if(kernel.currTaskId == 0) {
		currSP = kernel.tcbArray[nextTask].stack_ptr;
	} else { currSP = kernel.tcbArray[nextTask].stack_ptr; }

	__set_PSP(currSP);

	return;
}

void edfScheduler() {
	__disable_irq();

	task_t next = 0;
	int ignore = 0;

	if(kernel.currTaskId == -1) {
		next = 1;
	} else if(sleepingTasks != totalTasks){
		next = kernel.currTaskId;
	} else if (sleepingTasks == totalTasks){
		next = 0;
	}

	// calculate earliest deadline at the beginning and consider edge case for if the current task is sleeping or dormant
	int earliestDeadline = kernel.tcbArray[kernel.currTaskId].state == SLEEPING || kernel.tcbArray[kernel.currTaskId].state == DORMANT ? 2147483646 : kernel.tcbArray[kernel.currTaskId].deadline;

	// for the tasks that are sleeping
	for (int i = 1; i < MAX_TASKS; i++) {

		if(kernel.tcbArray[i].state == SLEEPING) {
			// decrement sleeping time
			kernel.tcbArray[i].sleepingTime --;

			// check if the task's sleeping time is 0, set it back to ready and reset it's deadline
			if(kernel.tcbArray[i].sleepingTime < 0) {
				kernel.tcbArray[i].state = READY;
				kernel.tcbArray[i].deadline = kernel.tcbArray[i].originalDeadline;
				sleepingTasks--;
			}
		} else if (kernel.tcbArray[i].state != DORMANT) {

			kernel.tcbArray[i].deadline --;

			// first check and update the earliest deadline once we decrement the deadline
			if(kernel.tcbArray[i].deadline < earliestDeadline || (kernel.tcbArray[i].deadline == earliestDeadline && i <= next)) {
				earliestDeadline = kernel.tcbArray[i].deadline;
				next = i;
			}

			// then we check if the deadline is 0 so that we don't miss a task being reset to it's original deadline
			if(kernel.tcbArray[i].deadline < 0) {
				kernel.tcbArray[i].deadline = kernel.tcbArray[i].originalDeadline;
			}
		}

	}

	nextTask = next;

	// if we need to context switch
	if (kernel.currTaskId != nextTask) {
		SCB->ICSR |= 1<<28; //control register bit for a PendSV interrupt
		__asm("isb");
		__enable_irq();
		return;
	} else {
		__enable_irq();
		return;
	}



}

int osSetDeadline(int deadline, task_t TID) {
	__disable_irq();
	int taskIndex = TID;
	if(kernel.currTaskId == taskIndex
			|| deadline <= 0
			|| TID <= 0
			|| TID >= 16
			|| kernel.tcbArray[taskIndex].state != READY) {
		return RTX_ERR;
	}

	kernel.tcbArray[taskIndex].deadline = deadline;
	kernel.tcbArray[taskIndex].originalDeadline = deadline;
	if(deadline < kernel.tcbArray[kernel.currTaskId].deadline && TID != kernel.currTaskId) { // If the newly set deadline is earlier than the current deadline, set next task to TID
		nextTask = TID;
		// reset the task's remaining time back to the deadline
		kernel.tcbArray[kernel.currTaskId].deadline = kernel.tcbArray[kernel.currTaskId].originalDeadline;

		// context switch to the newly set deadline task
		SCB->ICSR |= 1<<28; //control register bit for a PendSV interrupt
		__asm("isb");

	}

	__enable_irq();

	return RTX_OK;
}

void osSleep(int timeInMs) { 

	if(!isInitialized || !isRunning) {
		return;
	}
	// disable interrupts so that the context can be saved without any data corruption
	__disable_irq();

	// increment sleepingTasks global var
	sleepingTasks++;

	// save current running task's context set task status from running to sleeping
	kernel.tcbArray[kernel.currTaskId].state = SLEEPING;
	kernel.tcbArray[kernel.currTaskId].stack_ptr = __get_PSP();

	// Set the sleeping time for the task
    kernel.tcbArray[kernel.currTaskId].sleepingTime = timeInMs;

	// enable interrupts before triggering the SVC handler
	__enable_irq();

	edfScheduler();

}

int osCreateDeadlineTask(int deadline, TCB* task)
{
	// Return error if task stack size less than min stack size
	if (task == NULL || task->stack_size < STACK_SIZE || task->ptask == NULL || deadline <= 0)
	{
		return RTX_ERR;
	}

	// Main stack pointer initial value
	U32 *MSP_INIT_VAL = *(U32 **)0x0;

	// Check if memory is available, if not return error
	if (task->stack_size > kernel.availMemory)
	{
		return RTX_ERR;
	}

	// Finding a dormant task in the tcbArray
	int taskIndex = -1;


	// Max U16 value, we want to find the minimum stack size that accomodates our task
	U16 stack_size = 65535;

	//disable interrupts so that the task creation process doesn't get preempted in the middle and causes data corruption
	__disable_irq();

	for (int i = 1; i < MAX_TASKS; i++)
	{
		if (kernel.tcbArray[i].state == DORMANT && kernel.tcbArray[i].stack_size == 0)
		{
			taskIndex = i;
			break;
		}

		// Finding a dormant stack with a higher or equal stack_size value, that is the minimum
		// This is to prevent tasks from not being able to run
		// i.e. one task is on a stack that has 5000 bytes, but only requires 512. Another task requires 4000 bytes but can't find a suitable stack
		else if (kernel.tcbArray[i].state == DORMANT && kernel.tcbArray[i].stack_size >= task->stack_size && kernel.tcbArray[i].stack_size <= stack_size)
		{
			taskIndex = i;
			stack_size = kernel.tcbArray[i].stack_size;
			break;
		}
	}

	// Max amount of tasks scheduled
	if (taskIndex == -1)
	{
		return RTX_ERR;
	}


	// Assigning tid to task (REQUIRED)
	task->tid = taskIndex;

	// Creating a task in a never before allocated stack
	// These values stay the same otherwise, as we don't do any memory management apart from the first task created per stack
	if (stack_size == 65535)
	{
		// Calculate total stack size taken by other tasks
		U32 total_stack_size_taken = 0;
		for (int i = 0; i < taskIndex; i++)
		{
			if (kernel.tcbArray[i].state != DORMANT)
			{
				total_stack_size_taken += kernel.tcbArray[i].stack_size;
			}
		}

		kernel.tcbArray[taskIndex].stack_high = MSP_INIT_VAL - MAIN_STACK_SIZE - total_stack_size_taken;
		kernel.tcbArray[taskIndex].stack_size = task->stack_size;
	}

	// Setting task values to tcb array
	kernel.tcbArray[taskIndex].ptask = task->ptask;
	kernel.tcbArray[taskIndex].pc = task->ptask; // Set the program counter to the task entry point
	kernel.tcbArray[taskIndex].state = READY;
	kernel.tcbArray[taskIndex].tid = taskIndex;
	kernel.tcbArray[taskIndex].deadline = deadline; // 5ms default
	kernel.tcbArray[taskIndex].originalDeadline = deadline;


	// get pointer from alloc so we can access the data block struct to modify the owner tid
	void* newTaskPointer = k_mem_alloc(kernel.tcbArray[taskIndex].stack_size);

	if(newTaskPointer == NULL) {
		return RTX_ERR;
	}

	uintptr_t dum_adress = (uintptr_t)newTaskPointer - sizeof(data_Block);
	data_Block* buddy_block = (data_Block*)dum_adress;
	buddy_block->owner_tid = taskIndex;
	kernel.tcbArray[taskIndex].mem_ptr = newTaskPointer;

	// adding offset of the stack_size then doing modifications
	void* tmp = (uintptr_t) newTaskPointer + (uintptr_t) kernel.tcbArray[taskIndex].stack_size;

	// Calculating stack pointer
	kernel.tcbArray[taskIndex].stack_ptr = tmp;

	// Update available memory
	kernel.availMemory -= kernel.tcbArray[taskIndex].stack_size;


	*(--kernel.tcbArray[taskIndex].stack_ptr) = 1 << 24;							   // xPSR, setting chip to thumb mode
	*(--kernel.tcbArray[taskIndex].stack_ptr) = (U32)kernel.tcbArray[taskIndex].ptask;

	for (int i = 0; i < 14; i++)
	{
		*(--kernel.tcbArray[taskIndex].stack_ptr) = 0xA; // An arbitrary number
	}

	totalTasks++;

	// Special case when the created task pre-empts the task that created it
	if(kernel.tcbArray[taskIndex].deadline < kernel.tcbArray[kernel.currTaskId].deadline
		|| ((kernel.tcbArray[taskIndex].deadline == kernel.tcbArray[kernel.currTaskId].deadline) && (taskIndex < kernel.currTaskId))) {
		nextTask = taskIndex;
		if(isRunning) {
			 nextTask = taskIndex;
			 __enable_irq();
			 SCB->ICSR |= 1<<28; //control register bit for a PendSV interrupt
			 __asm("isb");
		}
	}

	//enable interrupts after creating the task so that there is no data corruption or preemption happening during the process of creating a task
	__enable_irq();

	return RTX_OK;
}

void osPeriodYield()
{

	if(!isInitialized || !isRunning) {
		return;
	}
	// disable interrupts so that the context can be saved without any data corruption
	__disable_irq();

	// increment sleepingTasks global var
	sleepingTasks++;

	// save current running task's context set task status from running to sleeping
	kernel.tcbArray[kernel.currTaskId].state = SLEEPING;

	// Set the sleeping time for the task
	kernel.tcbArray[kernel.currTaskId].sleepingTime = kernel.tcbArray[kernel.currTaskId].deadline;


	edfScheduler();

	return;
}