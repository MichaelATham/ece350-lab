/*
 * k_mem.c
 *
 *  Created on: Jun 23, 2024
 *      Author: kjmazurk
 */

#include "k_mem.h"
#include "common.h"
#include "k_task.h"
#include <stdio.h>

extern uint32_t _img_end;
extern uint32_t _Min_Stack_Size;
extern uint32_t _estack; // Get linker symbols defined by the Id script

static int isMemInitialized = 0;

static data_Block* free_block_list[MAX_ORDER+1]; // Head of the free block list with base case 16

static uint32_t heap_beg; // Where mem heap begins
static uint32_t heap_end; // Where mem heap ends

static U8 usage_map[MAX_HEAP_SIZE / MIN_BLOCK_SIZE];


//Helper functions below:
int order_finder(size_t size){ // Returns what order this block size is on
	int order = 0;
	int checker =32;

	while(checker < size){
		order++;
		checker = checker*2;
	}
	return order;
}

void free_add(data_Block* new_block){ //Adds a data block to the free list
	int order = order_finder(new_block->data_size); // Need to change this probably
	new_block->next = free_block_list[order];
	if (free_block_list[order] != NULL) {
		free_block_list[order]->prev = new_block;
	}
	new_block->prev = NULL;
	free_block_list[order] = new_block;
}

void free_remove(data_Block* block) {
	int order = order_finder(block->data_size);

    if (free_block_list[order] == block) {
    	free_block_list[order] = block->next;
    	if (block->next != NULL) {
    		block->next->prev = NULL;
    	}
    } else {
    	// If block is not the head, adjust pointers of neighboring nodes
    	if (block->prev != NULL) {
    		block->prev->next = block->next;
    	}
    	if (block->next != NULL) {
    		block->next->prev = block->prev;
    	}
    }
}


//Lab2 required functions below:
int k_mem_init(){
	if((get_isInitialized() == 0) || (isMemInitialized == 1)){
		return RTX_ERR;
	}
	// Init heap boundries:
	heap_beg = (uint32_t)&_img_end + 0x200;
	heap_end = (uint32_t)&_estack - (uint32_t)&_Min_Stack_Size;
	data_Block* init_block = (data_Block*)heap_beg;

	// Init Free lists:
	for (int i = 0; i < MAX_ORDER+1; i++) {
		free_block_list[i] = NULL;
	}

	// Since the whole heap starts as free add it to the Free table:
	init_block->data_size = MAX_HEAP_SIZE;
	init_block->owner_tid = -1;
	init_block->next = NULL;
	init_block->prev = NULL;
	free_block_list[MAX_ORDER] = init_block;


	isMemInitialized = 1;
	return RTX_OK;
}

void* k_mem_alloc(size_t size){
	if((isMemInitialized != 1) || (size == 0)){ //Check immediate failure conditions
		return NULL;
	}

	size_t req_size = size + sizeof(data_Block); // Size put into buddy system
	int order = order_finder(req_size); // Find the order of required size

	// Alloc Logic:
	for(int i = order; i <= MAX_ORDER; i++){
		if (free_block_list[i] != NULL) {
			data_Block* dummy_block = free_block_list[i];
			free_remove(dummy_block); // Hardfault happenes here becuase block or something gets overwritten

			while(i > order){
				i--;
				size_t block_size = MIN_BLOCK_SIZE << i;
				data_Block* buddy_block = (data_Block*)((uintptr_t)dummy_block + block_size);
				buddy_block->data_size = block_size;
				buddy_block->owner_tid = -1;
				buddy_block->next = NULL;
				buddy_block->prev = NULL;
				free_add(buddy_block);
			}

			//Adding required meta-data
			size_t usage_index = ((uintptr_t)dummy_block - heap_beg) / MIN_BLOCK_SIZE;
			usage_map[usage_index] = 1;
			dummy_block->data_size = size;
			dummy_block->owner_tid = osGetTID();
			return (void*)((uintptr_t)dummy_block + sizeof(data_Block));
		}
	}

	return NULL;
}

int k_mem_dealloc(void* ptr){
	int buddy_imp = 0;
	//Check for errors below:
	if(ptr == NULL){
		return RTX_ERR;
	}
	uintptr_t dum_adress = (uintptr_t)ptr - sizeof(data_Block);
	size_t index = (dum_adress - heap_beg) / MIN_BLOCK_SIZE;


	if ((dum_adress - heap_beg) % MIN_BLOCK_SIZE != 0) { // Check if pointers not aligned
		return RTX_ERR;
	}

	if((dum_adress > heap_end) || (dum_adress < heap_beg)){
		return RTX_ERR;
	}

	if(usage_map[index] == 0){//Check if mem is not the beginning of a used memory(for example it is invalid if ptr is in the middle of allocated mem)
		return RTX_ERR;
	}

	data_Block* dummy_block = (data_Block*)dum_adress;
	if(dummy_block->owner_tid != osGetTID()){ //If not owner return error
		return RTX_ERR;
	}

	//Add free block and set usage of block map leaf to 0:
	int block_size = sizeof(data_Block) + dummy_block->data_size;
	int free_block_order = order_finder(block_size);
	usage_map[index] = 0;
	dummy_block->data_size = MIN_BLOCK_SIZE << free_block_order;
	dummy_block->owner_tid = -1;
	dummy_block->next = NULL;
	dummy_block->prev = NULL;

	while (free_block_order < MAX_ORDER) {

		uintptr_t buddy_address = (((uintptr_t)dummy_block - heap_beg) ^ (MIN_BLOCK_SIZE << free_block_order)) + heap_beg;
		data_Block* buddy_block = (data_Block*)buddy_address;

		// Calculate buddy index
		size_t buddy_index = (buddy_address - heap_beg) / MIN_BLOCK_SIZE;
		// Check if the buddy block is free and within heap bounds
		if ((buddy_block->owner_tid != -1) || (buddy_address < heap_beg) || (buddy_address >= heap_end) || (usage_map[buddy_index] != 0)) {
			break;
		}
		// Check if the buddy block is in the free list
		int buddy_finder = 0;
		data_Block* curr_block = free_block_list[free_block_order];
		while (curr_block != NULL) {
			if (curr_block == buddy_block) {
				buddy_finder = 1;
				break;
			}
			curr_block = curr_block->next;
		}

		if (!buddy_finder) {
			break;
		}

		// Remove buddy block from free list
		free_remove(buddy_block);

		// Determine the starting address of the combined block
		if ((uintptr_t)dummy_block > (uintptr_t)buddy_block) {
			dummy_block = buddy_block;
		}

		// increase order
		free_block_order++;
	}

	// Set the size of the coalesced block
	dummy_block->data_size = MIN_BLOCK_SIZE << free_block_order;

	// Add the coalesced block back to the free list
	free_add(dummy_block);

	return RTX_OK;
}

int k_mem_count_extfrag(size_t size){
	if(isMemInitialized == 0){
		return 0;
	}
	int mem_counter = 0;
	int order = order_finder(size);
	if(order >10){
		order = 10 + 1;
	}
	data_Block* dummy_block;

	for(int i=0;i < order; i++){
		dummy_block = free_block_list[i];
		while(dummy_block != NULL){
			mem_counter++;
			dummy_block = dummy_block->next;
		}
	}

	return mem_counter;
}
