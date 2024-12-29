/*
 * k_mem.h
 *
 *  Created on: Jan 5, 2024
 *      Author: nexususer
 *
 *      NOTE: any C functions you write must go into a corresponding c file that you create in the Core->Src folder
 */
#include "queue.h"
#include "k_task.h"
#include "common.h"
#include "main.h"

#ifndef INC_K_MEM_H_
#define INC_K_MEM_H_

typedef unsigned int U32;
typedef unsigned short U16;
typedef char U8;
typedef unsigned int task_t;

#define MIN_BLOCK_ORDER 5
#define MIN_BLOCK_SIZE (1 << MIN_BLOCK_ORDER) // 32 byte min size specified
#define MAX_HEAP_SIZE (32 * 1024) // 32 KB for min requirement
#define BUDDY_SIZE (((2*MAX_HEAP_SIZE) / MIN_BLOCK_SIZE) - 1) // Calculates the amount of array space needed for the buddy tree
#define MAX_ORDER 10

typedef struct data_Block { //This also includes meta-data (16 bytes in total)
	size_t data_size;              // Size of the block (With the meta-data included)
    int owner_tid;      // Task owner id (-1 = free)
    struct data_Block* next;         // Pointer of the next free block
    struct data_Block* prev;         // Pointer of the previous free block
} data_Block;


int k_mem_init();
void* k_mem_alloc(size_t size);
int k_mem_dealloc(void* ptr);
int k_mem_count_extfrag(size_t size);

#endif /* INC_K_MEM_H_ */
