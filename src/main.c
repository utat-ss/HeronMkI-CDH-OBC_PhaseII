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

/* can_test0 includes */
//#include "can_test0.h"		Comment out the other can_tests to use the one you want.

/* can_test1 includes */
//#include "can_test1.h"

/* rtt_test0 includes */
#include "rtt_test0.h"

#include "can_func.h"

/*
* my_blink() is used when PROGRAM_CHOICE is set to 1.
* main_blinky() is used when PROGRAM_CHOICE is set to 2.
* can_test() is used when PROGRAM_CHOICE is set to 3.
* can_test1() is used when PROGRAM_CHOICE is set to 4.
* rtt_test0() is used when PROGRAM_CHOICE is set to 5.
* housekeep_test() is used when PROGRAM_CHOICE is set to 6.
* stk600_test0() is used when PROGRAM_CHOICE is set to 7.
* command_test() is used when PROGRAM_CHOICE is set to 8
* main_full() is used when PROGRAM_CHOICE is set to 9.
*/
#define PROGRAM_CHOICE	8
/*-----------------------------------------------------------*/

/*
* Set up the hardware ready to run this demo.
*/
static void prvSetupHardware(void);

extern void main_blinky(void);
extern void main_full(void);
extern void my_blink(void);
//extern void can_test(void);
extern void housekeep_test(void);
extern void stk600_test0(void);
extern void command_test(void);

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
	/* Prepare the hardware to run this demo. */
	prvSetupHardware();

#if PROGRAM_CHOICE == 1
	{
		gpio_toggle_pin(LED2_GPIO);
		my_blink();
		vTaskStartScheduler();
	}
#endif
#if PROGRAM_CHOICE == 2
	{
		main_blinky();
	}
#endif
#if PROGRAM_CHOICE == 3
	{
//		can_test0();
	}
#endif
#if PROGRAM_CHOICE == 4
	{
		can_test1();
	}
#endif
#if PROGRAM_CHOICE == 5
	{
		rtt_test0();
	}
#endif
#if PROGRAM_CHOICE == 6
	{
		my_blink();
		housekeep_test();
		vTaskStartScheduler();
		while(1) { }
	}
#endif
#if PROGRAM_CHOICE == 7
	{
		stk600_test0();
	}
#endif
#if PROGRAM_CHOICE == 8
{
	command_test();
}
#endif
#if PROGRAM_CHOICE == 9
	{
		main_full();
	}
#endif
	{
		while (1){}
	}
	return 0;
}
/*-----------------------------------------------------------*/

/**
 * \brief Initializes the hardware.	
 */
static void prvSetupHardware(void)
{
	extern void SystemCoreClockUpdate(void);

	/* ASF function to setup clocking. */
	sysclk_init();

	/* Ensure all priority bits are assigned as preemption priority bits. */
	NVIC_SetPriorityGrouping(0);

	/* Atmel library function to setup for the evaluation kit being used. */
	board_init();

	/* Perform any configuration necessary to use the ParTest LED output
	functions. */
	vParTestInitialise();
	
	/* Initialize CAN-related registers and functions for tests and operation */
	can_initialize();
	
}
/*-----------------------------------------------------------*/


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

