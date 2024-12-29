.syntax unified
.cpu cortex-m4
.fpu softvfp
.thumb

.global PendSV_Handler // indicates to the linker that this function exists

.thumb_func //ensures that the address of the interrupt function is properly aligned

PendSV_Handler: //function name

.global SVC_Handler_Main

//code for the function here

// Checks if MSP or PSP and loads to R0
TST lr, #4
ITE EQ
MRSEQ r0, MSP
MRSNE r0, PSP

// Save r4-r11 to the task's stack
STMDB r0!, {r4-r11}

//Moves the value of r0 (since it decremented it with stmdb) into PSP
MSR PSP, r0

// Branch to SVC Handler
BL SVC_Handler_Main

// Store new PSP into R0
MRS r0, PSP

// Loading registers r4-r11

LDMIA r0!, {r4-r11}


// Setting the PSP back to r0 as it incremented
MSR PSP, r0


LDR LR, =0xFFFFFFFD
BX LR
