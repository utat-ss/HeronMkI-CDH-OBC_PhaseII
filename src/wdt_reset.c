
/*
    Author: Ali Haydaroglu

	***********************************************************************
	*	FILE NAME:		wdt_reset.c
	*
	*	PURPOSE:		
	*	This file is to be used to create a low-priority function to reset the watchdog timer
	*
	*	FILE REFERENCES:	stdio.h, FreeRTOS.h, task.h, partest.h, asf.h, 
	*						These file references were just taken from housekeeping.c. 
	*						I don't know what I need or don't need
	*
	*	EXTERNAL VARIABLES: Not sure what these mean
	*
	*	EXTERNAL REFERENCES:	Same a File References (?)
	*
	*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
	*
	*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None yet.
	*
	*	NOTES:	This is my first assignment, I'm really not sure if anything on here 
	*			is the way it's supposed to be. 
	*	
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:			
	*	New tasks should be written to use as much of CMSIS as possible. The ASF and 
	*	FreeRTOS API libraries should also be used whenever possible to make the program
	*	more portable.

	*	DEVELOPMENT HISTORY:		
	*	10/10/2015		Created.
	*
	*	DESCRIPTION:
	*	
	*	Currently, this file simply creates an empty task which delays itself on running. 
	*	Watchdog timer reset functionality will be added.
	*	
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

//What other includes do I need?

#include <asf/sam/drivers/wdt/wdt.h>


/* Priorities at which the task is created. */
#define WDT_Reset_PRIORITY        (tskIDLE_PRIORITY + 1)
/* all other task priorities need to be incremented */

/* Time that task is delayed for after running. Should this be here? */
#define WDT_Reset_Delay				   100 
/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define WDT_PARAMETER			( 0xABCD ) // What is this?

/*-------------------------------------------------------------*/

/* Functions Prototypes */
static void wdtResetTask( void *pvParameter); // I don't know what this does
TaskHandle_t wdt_reset(void);
void wdt_reset_kill(uint8_t killer);

/*-------------------------------------------------------------*/

TaskHandle_t wdt_reset(void)
{
	/*Start the watchdog timer reset task as described in comments */
	TaskHandle_t temp_HANDLE = 0;
	xTaskCreate( wdtResetTask,			/* The function that implements the task. */
	"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
	configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
	( void * ) WDT_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
	WDT_Reset_PRIORITY, 			    /* The priority assigned to the task. */
	&temp_HANDLE );								/* The task handle is not required, so NULL is passed. */
	
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	return temp_HANDLE;
	
	//What happens if it does return? //death and destruction of course.
}

/************************************************************************/
/*				WDT RESET TASK		                                */
/*	The purpose of this task is to reset the watchdog timer every  	*/
/*	time interval (defined by WDT_Reset_Delay)						*/
/************************************************************************/

static void wdtResetTask(void *pvParameters)
{
	//What is this?
	configASSERT( ( ( unsigned long ) pvParameters ) == WDT_PARAMETER );
	
	TickType_t xLastWakeTime;
	
	//is this okay?
	const TickType_t xTimeToWait = WDT_Reset_Delay;
	
	/* @non-terminating@ */	
	
	for ( ;; )
	{
		wdt_restart(WDT);
		
		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);	
	}
}

// This function will kill this task.
// If it is being called by this task 0 is passed, otherwise it is probably the FDIR task and 1 should be passed.
void wdt_reset_kill(uint8_t killer)
{
	// Kill the task.
	if(killer)
		vTaskDelete(wdt_reset_HANDLE);
	else:
		vTaskDelete(NULL);
	return;
}