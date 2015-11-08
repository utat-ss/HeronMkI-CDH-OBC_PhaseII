/*
Author: Keenan Burnett, Omar Abdeldayem
***********************************************************************
* FILE NAME: time_manage
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
*				Absolute time is now being stored in spi memory.
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
* 11/07/2015	I changed the name of this file from time_manage.c to time_manage.c to better indicate
*				what this file is meant for.
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
#define TIME_MANAGE_PRIORITY		( tskIDLE_PRIORITY + 1 )		// Lower the # means lower the priority

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define TIME_MANAGE_PARAMETER			( 0xABCD )

/* Definitions to clarify which service subtypes represent what	*/
/* Time									*/
#define UPDATE_REPORT_FREQ				1
#define TIME_REPORT						2

/*		Functions Prototypes.		*/
static void prvTimeManageTask( void *pvParameters );
void time_manage(void);
static void broadcast_minute(void);
static void update_absolute_time(void);
static void report_time(void);
static void exec_commands(void);

/* Local Variables for Time Management */
static struct timestamp time;
static uint32_t minute_count;
static uint32_t report_timeout;
static uint8_t current_command[2];

/************************************************************************/
/* time_manage (Function)												*/
/* @Purpose: This function is used to create the time update task.		*/
/************************************************************************/
void time_manage( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvTimeManageTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) TIME_MANAGE_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					TIME_MANAGE_PRIORITY, 			/* The priority assigned to the task. */
					NULL );								/* The task handle is not required, so NULL is passed. */
	return;
}
/*-----------------------------------------------------------*/

/************************************************************************/
/*				TIME MANAGE TASK		                                */
/*	The sole purpose of this task is to send a single CAN containing	*/
/*	the current minute from the RTC, every minute						*/
/************************************************************************/
static void prvTimeManageTask( void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == TIME_MANAGE_PARAMETER );
	minute_count = 0;
	report_timeout = 60;	// Produce a time report once every 60 minutes.
	
	/* @non-terminating@ */	
	for( ;; )
	{
		if (rtc_triggered_a2())
		{
			rtc_get(&time);
			minute_count++;
			if(minute_count == report_timeout)
				report_time();
			
			update_absolute_time();
			broadcast_minute();

			rtc_reset_a2();
		}
		exec_commands();
	}
}
/*-----------------------------------------------------------*/

static void broadcast_minute(void)
{
	uint32_t high;
	high = high_command_generator(TIME_TASK_ID, EPS_ID, MT_TC, SET_TIME);
	send_can_command_h((uint32_t)CURRENT_MINUTE, high, SUB1_ID0, DEF_PRIO);
	high = high_command_generator(TIME_TASK_ID, COMS_ID, MT_TC, SET_TIME);
	send_can_command_h((uint32_t)CURRENT_MINUTE, high, SUB0_ID0, DEF_PRIO);
	high = high_command_generator(TIME_TASK_ID, PAY_ID, MT_TC, SET_TIME);
	send_can_command_h((uint32_t)CURRENT_MINUTE, high, SUB2_ID0, DEF_PRIO);
	return;
}

// Updates the global variables which store absolute time and stores it in SPI memory every minute.
// This function updates absolute time, and stores it in SPI memory for safe keeping.
static void update_absolute_time(void)
{
	CURRENT_MINUTE = time.minute;
	if(time.hour != CURRENT_HOUR)
	{
		if(CURRENT_HOUR == 23)
			ABSOLUTE_DAY++;
		CURRENT_HOUR = time.hour;
	}
	CURRENT_SECOND = time.sec;
	
	absolute_time_arr[0] = ABSOLUTE_DAY;
	absolute_time_arr[1] = CURRENT_HOUR;
	absolute_time_arr[2] = CURRENT_MINUTE;
	absolute_time_arr[3] = CURRENT_SECOND;
	
	spimem_write(TIME_BASE, absolute_time_arr, 4);	// Writes the absolute time to SPI memory.
	return;
}

static void report_time(void)
{
	xQueueSendToBack(time_to_obc_fifo, absolute_time_arr, (TickType_t)1);		// FAILURE_RECOVERY if this doesn't return pdTrue
	minute_count = 0;
	return;
}

// Execute commands that are sent from the obc_packet_router
static void exec_commands(void)
{
	if(xQueueReceive(obc_to_time_fifo, current_command, (TickType_t)10) == pdTRUE)
	{
		report_timeout = current_command[0];
	}
	return;
}
