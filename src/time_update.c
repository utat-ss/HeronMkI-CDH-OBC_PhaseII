/*
 * update_sat_time.c
 *
 * Created: 8/29/2015 15:49:07
 *  Author: Omar
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

/************************************************************************/
/*			TEST FUNCTION FOR COMMANDS TO THE STK600                    */
/************************************************************************/
/**
 * \brief Tests the housekeeping task.
 */
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
/**
 * \brief Performs the housekeeping task.
 * @param *pvParameters:
 */
static void prvTimeUpdateTask( void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == TIME_UPDATE_PARAMETER );
	
	timestamp time;
	uint32_t high;

	/* @non-terminating@ */	
	for( ;; )
	{
		if (rtc_triggered_a2())
		{
			rtc_get(&time);
			
			CURRENT_MINUTE = time.minute;
			
			high = high_command_generator(TC_TASK_ID, MT_TC, SET_TIME);
			
			send_can_command((uint32_t)CURRENT_MINUTE, high, EPS_ID, DEF_PRIO);
			send_can_command((uint32_t)CURRENT_MINUTE, high, COMS_ID, DEF_PRIO);
			send_can_command((uint32_t)CURRENT_MINUTE, high, PAY_ID, DEF_PRIO);
			
			rtc_reset_a2();
		}
	}
}
/*-----------------------------------------------------------*/