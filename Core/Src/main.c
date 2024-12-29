/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h> //You are permitted to use this library, but currently only printf is implemented. Anything else is up to you!
#include "common.h"
#include "k_task.h"
#include "k_mem.h"

extern isRunning;
int switches = 0;

// // Exercise 3
// void print_continuously(void)
// {
// 	task_t current_task_id = osGetTID();
// 	printf("Current Task ID: %d\r\n", current_task_id);
// 	osYield();
// 	printf("Yielded Task ID: %d\r\n", current_task_id);
// 	osTaskExit();
// }

// Lab 3 Prelab
// void loop_continuously()
// {
// 	while(1) {
// 		task_t current_task_id = osGetTID();
// 		printf("Current Task ID: %d\r\n", current_task_id);

// 		if(osCheckTime(timeslice)) {
// 			timeslice = 0;
// 			osYield();
// 		}
// 	}
// }

// // Lab 3 Prelab
// void loop_continuously_with_yield()
// {
// 	while(1) {
// 		task_t current_task_id = osGetTID();
// 		TCB curr_task;
// 		osTaskInfo(20, &curr_task);
// 		printf("Current Task ID: %d\r\n", current_task_id);

// 		if(osCheckTime(timeslice)) {
// 			timeslice = 0;
// 			switches++;
// 			osYield();
// 		}

// 		if(switches == 2) {
// 			if(osCheckTime(timeslice * 2)) {
// 				timeslice = 0;
// 				osYield();
// 				osTaskExit();
// 			}
// 		}
// 	}
// }

// PERIODIC TEST

//int i_test = 0;
//
//int i_test2 = 0;
//
//
//void TaskA(void *) {
//   while(1){
//      printf("%d, %d\r\n", i_test, i_test2);
//      osPeriodYield();
//   }
//}
//
//void TaskB(void *) {
//   while(1){
//      i_test = i_test + 1;
//      osPeriodYield();
//   }
//}
//
//void TaskC(void *) {
//   while(1){
//      i_test2 = i_test2 + 1;
//      osPeriodYield();
//   }
//}


//BACKWARDS COMPATIBILITY
void Task1(void *) {
   while(1){
     printf("1\r\n");
     for (int i_cnt = 0; i_cnt < 5000; i_cnt++);
     osYield();
   }
}


void Task2(void *) {
   while(1){
     printf("2\r\n");
     for (int i_cnt = 0; i_cnt < 5000; i_cnt++);
     osYield();
   }
}


void Task3(void *) {
   while(1){
     printf("3\r\n");
     for (int i_cnt = 0; i_cnt < 5000; i_cnt++);
     osYield();
   }
}

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

  /* MCU Configuration: Don't change this or the whole chip won't work!*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* Configure the system clock */
  SystemClock_Config();


  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* MCU Configuration is now complete. Start writing your code below this line */

  /* PRE-LAB CODE STARTS HERE */

  // Clearing entire screen
    printf("\033[2J");
    osKernelInit();

    //OTHER TEST
    //    task.ptask = loop_continuously;
    //    task.stack_high = 0;
    //    task.tid = 0;
    //    task.state = 0;
    //    task.stack_size = 512;
    //    task.deadline = 5000;
    //
    //    task2.ptask = loop_continuously;
    //    task2.stack_high = 0;
    //    task2.tid = 0;
    //    task2.state = 0;
    //    task2.stack_size = 512;
    //    task2.deadline = 5000;
    //
    //    task3.ptask = loop_continuously;
    //    task3.stack_high = 0;
    //    task3.tid = 0;
    //    task3.state = 0;
    //    task3.stack_size = 512;
    //    task3.deadline = 5000;

    //    res = osCreateTask(&task);
    //    osCreateTask(&task2);
    //    osCreateTask(&task3);
    //    printf("Created Task Result: %d \r\n", res);
    //
    //    TCB task_copy;
    //    res = osTaskInfo(20, &task_copy);
    //    printf("Info: %d \r\n", res);
    //
    //    res = osTaskInfo(1, &task_copy);
    //    printf("Info: %d \r\n", res);
    //    printf("Stack size: %d \r\n", task_copy.stack_size);

    // PERIODIC TESET

//    TCB st_mytask;
//    st_mytask.stack_size = STACK_SIZE;
//    st_mytask.ptask = &TaskA;
//    osCreateDeadlineTask(4, &st_mytask);
//
//    st_mytask.ptask = &TaskB;
//    osCreateDeadlineTask(4, &st_mytask);
//
//    st_mytask.ptask = &TaskC;
//    osCreateDeadlineTask(12, &st_mytask);



    //BACKWARDS COMPATIBILITY
    TCB st_mytask;
      st_mytask.stack_size = STACK_SIZE;
      st_mytask.ptask = &Task1;
      osCreateTask(&st_mytask);


      st_mytask.ptask = &Task2;
      osCreateTask(&st_mytask);


      st_mytask.ptask = &Task3;
      osCreateTask(&st_mytask);



	printf("Reset\r\n");
    osKernelStart();


  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
  // Prelab 3 Exercise
	// if(timeslice == 0){
	// 	timeslice = 5000;
	// 	printf("Counter done.\n");
	// }
    /* USER CODE END WHILE */
    // 	printf("Hello, world!\r\n");
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}
