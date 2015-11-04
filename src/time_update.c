/*
Author: Keenan Burnett, Omar Abdeldayem
***********************************************************************
* FILE NAME: time_update
*
* PURPOSE:
* This file is to be used to house the time-management task (which shall manage
* all time-related activities.
*
* FILE REFERENCES: stdio.h, FreeRTOS.h, task.h, partest.h, asf.h, can_func.h
*
* EXTERNAL VARIABLES:
*
* EXTERNAL REFERENCES: Same a File References.
*
* ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
*
* ASSUMPTIONS, CONSTRAINTS, CONDITIONS: None
*
* NOTES:
*
*				This task is soon going to turn into the "time management" task.
*
*				How to keep track of absolute time on the spacecraft?
*
* REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
* DEVELOPMENT HISTORY:
* 08/29/2015 	O: Created.
*
* 10/25/2015	K: I added the code to the SSM programs so that a CAN message may be sent in order to
*				update the CURRENT_MINUTE variable stored in the SSM.
*
*				I also added in the code here so that it sends out a CAN message to all the SSMs
*				every minute (in order to update CURRENT_MINUTE).
*
* DESCRIPTION:
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

/*		RTC includes.	*/
#include "rtc.h"

#include "can_func.h"

#include "global_var.h"

/* Priorities at which the tasks are created. */
#define TIME_UPDATE_PRIORITY		( tskIDLE_PRIORITY + 1 )		// Lower the # means lower the priority

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define TIME_UPDATE_PARAMETER			( 0xABCD )

/*
 * Functions Prototypes.
 */
static void prvTimeUpdateTask( void *pvParameters );
void time_update(void);

struct timestamp time;

/************************************************************************/
/* TIME_UPDATE (Function)												*/
/* @Purpose: This function is used to create the time update task.		*/
/************************************************************************/
void time_update( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvTimeUpdateTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) TIME_UPDATE_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					TIME_UPDATE_PRIORITY, 			/* The priority assigned to the task. */
					NULL );								/* The task handle is not required, so NULL is passed. */
	return;
}
/*-----------------------------------------------------------*/

/************************************************************************/
/*				TIME UPDATE TASK		                                */
/*	The sole purpose of this task is to send a single CAN containing	*/
/*	the current minute from the RTC, every minute						*/
/************************************************************************/
static void prvTimeUpdateTask( void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == TIME_UPDATE_PARAMETER );
	uint32_t high;

	/* @non-terminating@ */	
	for( ;; )
	{
		if (rtc_triggered_a2())
		{
			rtc_get(&time);
			
			CURRENT_MINUTE = time.minute;
			
			high = high_command_generator(TC_TASK_ID, EPS_ID, MT_TC, SET_TIME);
			send_can_command_h((uint32_t)CURRENT_MINUTE, high, SUB1_ID0, DEF_PRIO);
			high = high_command_generator(TC_TASK_ID, COMS_ID, MT_TC, SET_TIME);
			send_can_command_h((uint32_t)CURRENT_MINUTE, high, SUB0_ID0, DEF_PRIO);
			high = high_command_generator(TC_TASK_ID, PAY_ID, MT_TC, SET_TIME);
			send_can_command_h((uint32_t)CURRENT_MINUTE, high, SUB2_ID0, DEF_PRIO);
			
			rtc_reset_a2();
		}
	}
}
/*-----------------------------------------------------------*/