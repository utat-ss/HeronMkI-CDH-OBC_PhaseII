/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		memory_wash.c
	*
	*	PURPOSE:		
	*	This file is to be used to create the housekeeping task needed to monitor
	*	housekeeping information on the satellite.
	*
	*	FILE REFERENCES:	stdio.h, FreeRTOS.h, task.h, partest.h, asf.h, can_func.h
	*
	*	EXTERNAL VARIABLES:
	*
	*	EXTERNAL REFERENCES:	Same a File References.
	*
	*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
	*
	*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
	*
	*	NOTES:	 
	*	
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:			
	*	New tasks should be written to use as much of CMSIS as possible. The ASF and 
	*	FreeRTOS API libraries should also be used whenever possible to make the program
	*	more portable.

	*	DEVELOPMENT HISTORY:		
	*	10/15/2015		Created.
	*
	*	DESCRIPTION:	
	*
	*	This task shall periodically "wash" the external SPI Memory by checking for bit flips and
	*	subsequently correcting them.
	*
	*	In order to do this, we intend to have 3 external SPI Memory chips and use a "voting"
	*	method in order to correct bit flips on the devices.
	*	
 */

/* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Atmel library includes. */
#include "asf.h"

/* Common demo includes. */
#include "partest.h"

/* CAN Function includes */
#include "can_func.h"

/* Priorities at which the tasks are created. */
#define MemoryWash_PRIORITY		( tskIDLE_PRIORITY + 1 )		// Lower the # means lower the priority

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define MW_PARAMETER			( 0xABCD )

/*-----------------------------------------------------------*/

/*
 * Functions Prototypes.
 */
static void prvMemoryWashTask( void *pvParameters );
void menory_wash(void);

/************************************************************************/
/* MEMORY_WASH (Function)												*/
/* @Purpose: This function is used to create the memory washing task.	*/
/************************************************************************/
void memory_wash( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvMemoryWashTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) MW_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					MemoryWash_PRIORITY, 			/* The priority assigned to the task. */
					NULL );								/* The task handle is not required, so NULL is passed. */
					
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	return;
}

/************************************************************************/
/*				MEMORY WASH (TASK)		                                */
/*	This task will periodically take control of the SPI Memory Chips and*/
/* verify that there are no bit flips within them by way of a voting    */
/* algorithm between 3 separate chips storing redundant data.			*/
/************************************************************************/
static void prvMemoryWashTask(void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == MW_PARAMETER );
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 1;	// Number entered here corresponds to the number of ticks we should wait.
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
	
	uint32_t ID, x;
	uint8_t passkey = 0, addr = 0x80;
		
	/* @non-terminating@ */	
	for( ;; )
	{
		// Memory washing here.
		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
	}
}
/*-----------------------------------------------------------*/

