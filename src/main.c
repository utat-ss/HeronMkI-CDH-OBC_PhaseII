/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved

Author: Keenan Burnett
***********************************************************************
*	FILE NAME:		main.c
*
*	PURPOSE:
*	This is the main program that is first called when the program is
*	loaded onto the ATSAM3X. prvSetupHardware() calls several other
*	subroutines which are used for initialization.
*
*	FILE REFERENCES:	stdio.h, FreeRTOS.h, task.h, partest.h, asf.h
*			(By extension, almost everything is linked to main.c)
*
*	EXTERNAL VARIABLES:
*
*	EXTERNAL REFERENCES:	(Many), see the dependencies diagram on the dropbox.
*
*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: 
*
*	Sometimes the WDT does not run out even when it is initialized, the problem
*	seems to be specific to this program only. NEEDS TO BE DEALT WITH.
*
*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
*
*	NOTES:
*	The bootloader is burned onto a ROM which is used to
*	program the ATMSAM3X on the Arduino DUE. Other than that, you can find
*	pin assignments in sam3x_ek and sam3x_8e.
*
*	Within conf_board.h, CONF_BOARD_KEEP_WATCHDOG_AT_INIT can either be defined
*	or commented out. If defined, the watchdog timer will be initialized and will
*	blink a given LED every time the timer times out.
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*	New tasks should be written to use as much of CMSIS as possible. The ASF and
*	FreeRTOS API libraries should also be used whenever possible to make the program
*	more portable.

*	DEVELOPMENT HISTORY:
*	11/15/2014		Set maincreate_SIMPLE_BLINKY_DEMO_ONLY to 1.
*
*	11/16/2014		Changed how LED's were toggled before main_blinky() was called.
*
*	11/30/2014		Ported the watchdog timer example over to the main program.
*
*	12/20/2014		Ported the CAN example over to the main program.
*					Changed maincreate_SIMPLE_BLINKY_DEMO_ONLY to PROGRAM_CHOICE.
*
*	12/21/2014		Added can_test0.c to the main program folder, this is another test
*					program which can be run to test CAN functionality.
*
*					Changed PROGRAM_CHOICE to PROGRAM_CHOICE.
*
*	12/24/2014		Included can_test1.h which corresponds to program choice can_test1.
*
*	12/27/2014		PROGRAM_CHOICE is currently 4 => can_test1().
*
*	12/31/2014		Added rtt.c and rtt.h to the project which shall be used for the purpose of housekeeping.
*
*	01/01/2015		PROGRAM_CHOICE is currently 5 => rtt().
*
*	01/02/2015		can_initialize is now a part of prvSetupHardware()
*
*	01/03/2015		housekeep_test.c has been added to the project and is now working. CAN can now be considered
*					operation as regular messages and remote frames can be sent between controllers 0 and 1.
*
*					PROGRAM_CHOICE is currently 6 => housekeep_test()
*
*	01/24/2015		stk600_test0.c has now been added to the project in order to test communication with
*					our newly acquired development board which we are using to simulate a subsystem MCU.
*
*					PROGRAM_CHOICE is currently 7 => stk600_test0()
*
*	02/01/2015		Today I have been working on getting both sending and receiving to work between the STk600
*					and the Arduino DUE.
*
*					I have created a new test file called command_test.c which I am using to test some CAN API 
*					functions that I have written.
*
*					PROGRAM_CHOICE is now set to 8 in order to run command_test()
*
*	02/17/2015		Today I'm working on getting sending and receiving working between the STK600 and the Arduino DUE.
*
*					I am also working on getting housekeeping to work reliably with the subsystem micro which means
*					getting remote messages working over the CAN bus (these already work between CAN0 and CAN1).
*
*	02/18/2015		Today I got housekeeping working with the STK600 and have now created two repositories on which I 
*					will be working. This repository (PhaseII) shall be used as a prototype for the actual flight 
*					software which shall be run on our satellite.
*
*					I am now removing several test programs from this repository.
*
*	07/12/2015		Instead of jumping right into normal operation, we will first enter into what we are calling 'safe mode'
*					We start off by executing the least amount of code possible in order to get the OBC into a state where
*					it is waiting for a CAN message that will allow it to enter into regular operation. In main(), the first
*					function that will be executed is now safe_mode().
*
*	DESCRIPTION:
*	This is the 'main' file for our program which will run on the OBC.
*	main.c is called from the reset handler and will initialize hardware,
*	create tasks and initialize interrupts.
*	The creation of tasks is segregated into separate files so that
*	logically related tasks are separated.
*	main_full and main_blink are demos files created by ATMEL.
*	my_blink is a demo that I created myself for periodically checking system
*	functionality with the use of LEDs (changes frequently)
*
*/

/* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Standard demo includes - just needed for the LED (ParTest) initialization
function. */
#include "partest.h"

/* Atmel library includes. */
#include <asf.h>

#include "can_func.h"

#include "usart_func.h"

#include "spi_func.h"

#include "rtc.h"

uint32_t data_reg[2];

#include "global_var.h"

/* MUTEXES and SEMAPHORES */

/* Set up the hardware ready to run the program. */
static void prvSetupHardware(void);

/*	Initialize mutexes and semaphores to be used by the programs  */
static void prvInitializeMutexes(void);

/*  Initialize the priorities of various interrupts on the Cortex-M3 System */
static void prvInitializeInterruptPriorities(void);

/*	This is the initial state of operation. Here we wait for go-ahead from a groundstation.*/
static void safe_mode(void);

/* External functions used to create and encapsulate different tasks*/

extern void my_blink(void);
extern void command_loop(void);
extern void housekeep(void);
extern void data_test(void);
extern void	spi_initialize(void);

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook(void);
void vApplicationIdleHook(void);
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName);
void vApplicationTickHook(void);

/*-----------------------------------------------------------*/

/* See the documentation page for this demo on the FreeRTOS.org web site for
full information - including hardware setup requirements. */

/************************************************************************/
/*								MAIN                                    */
/*		This is the function that is called when the reset handler		*/
/*		is triggered.													*/
/************************************************************************/

int main(void)
{
	SAFE_MODE = 0;
	safe_mode();
	
	/* Initialize Interrupt Priorities */
	prvInitializeInterruptPriorities();
	
	/* Prepare the hardware */
	prvSetupHardware();
	
	/* Initialize Mutexes */
	prvInitializeMutexes();
	
	/* Create Tasks */
	my_blink();
	housekeep();
	command_loop();
	data_test();
	
	/* Start Scheduler */
	vTaskStartScheduler();
	
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for tasks	to be created.*/
	
	while (1);
			
	return 0;
}
/*-----------------------------------------------------------*/

static void safe_mode(void)
{
	extern void SystemCoreClockUpdate(void);
	
	/* ASF function to setup clocking. */
	sysclk_init();
	
	/* Ensure all priority bits are assigned as preemption priority bits. */
	NVIC_SetPriorityGrouping(0);
	
	/* Initializes WDT, CAN, and interrupts. */
	safe_board_init();
	
	/* Initialize CAN-related registers and functions for tests and operation */
	can_initialize();
	
	//hash_mem();
	
	// if (has_result == stored_hash_result)
		// We're good, SAFE_MODE = 0
		
	// else	
		//send_can_command(result_of_hash, to_coms);
	
	while(SAFE_MODE){}		// We should remain here until this variable is updated
							// by the interrupt.
}


/**
 * \brief Initializes the hardware.	
 */
static void prvSetupHardware(void)
{
	/* Perform the remainder of board initialization functions. */
	board_init();

	/* Perform any configuration necessary to use the ParTest LED output functions. */
	vParTestInitialise();
		
	/* Initialize USART-related registers and functions. */
	usart_initialize();
	
	/* Initilize SPI related registers and functions. */
	spi_initialize();
	
	/* Initialize RTC registers and set the default initial time. */
	rtc_init(DS3234_INTCN);
}
/*-----------------------------------------------------------*/

static void prvInitializeMutexes(void)
{	
	Can1_Mutex = xSemaphoreCreateBinary();
	return;
}

static void prvInitializeInterruptPriorities(void)
{
	uint32_t priority = 11;
	IRQn_Type can1_int_num = (IRQn_Type)44;
	IRQn_Type can0_int_num = (IRQn_Type)43;
		
	NVIC_SetPriority(can1_int_num, priority);
	
	priority = 12;	
	NVIC_SetPriority(can0_int_num, priority);
	
	priority = NVIC_GetPriority(can1_int_num);
	
	return;
}

void vApplicationMallocFailedHook(void)
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created.  It is also called by various parts of the
	demo application.  If heap_1.c or heap_2.c are used, then the size of the
	heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */
	taskDISABLE_INTERRUPTS();
	/* @non-terminating@ */
	for (;;);
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook(void)
{
	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
	task.  It is essential that code added to this hook function never attempts
	to block in any way (for example, call xQueueReceive() with a block time
	specified, or call vTaskDelay()).  If the application makes use of the
	vTaskDelete() API function (as this demo application does) then it is also
	important that vApplicationIdleHook() is permitted to return to its calling
	function, because it is the responsibility of the idle task to clean up
	memory allocated by the kernel to any task that has since been deleted. */
}
/*-----------------------------------------------------------*/

/**
 * \brief 
 */
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
	(void)pcTaskName;
	(void)pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	/* @non-terminating@ */
	for (;;);
}
/*-----------------------------------------------------------*/

void vApplicationTickHook(void)
{
	/* This function will be called by each tick interrupt if
	configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
	added here, but the tick hook is called from an interrupt context, so
	code must not attempt to block, and only the interrupt safe FreeRTOS API
	functions can be used (those that end in FromISR()). */
}
/*-----------------------------------------------------------*/

/*---------------CUSTOM INTERRUPT HANDLERS-------------------*/

/**
 * \brief Clears watchdog timer status bit and restarts the counter.
 */
void WDT_Handler(void)
{
	/* Clear status bit to acknowledge interrupt by dummy read. */
	wdt_get_status(WDT);
	gpio_toggle_pin(LED1_GPIO);
	/* Restart the WDT counter. */
	wdt_restart(WDT);
}

/*-----------------------------------------------------------*/

