/*
Author: Keenan Burnett, Omar Abdeldayem
***********************************************************************
* FILE NAME: time_manage
*
* PURPOSE:
* This file is to be used to house the time-management task (which shall manage
* all time-related activities.
*
* FILE REFERENCES: stdio.h, FreeRTOS.h, task.h, partest.h, asf.h, can_func.h, error_handling.h
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
* 01/15/2015    A: Added wrapper function to handle FIFO errors.
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

/*    Error handling includes   */
#include "error_handling.h"

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
/* Time									
#define UPDATE_REPORT_FREQ				1
#define TIME_REPORT						2*/

/*		Functions Prototypes.		*/
static void prvTimeManageTask( void *pvParameters );
TaskHandle_t time_manage(void);
void time_manage_kill(uint8_t killer);
void broadcast_minute(void);
void update_absolute_time(void);
void report_time(void);
static void exec_commands(void);
static void send_tc_execution_verify(uint8_t status, uint16_t packet_id, uint16_t psc);

/* Local Variables for Time Management */
static struct timestamp time;
static uint32_t minute_count;
static uint32_t report_timeout;
static uint8_t current_command[10];

/************************************************************************/
/* time_manage (Function)												*/
/* @Purpose: This function is used to create the time update task.		*/
/************************************************************************/
TaskHandle_t time_manage( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		TaskHandle_t temp_HANDLE = 0;
		xTaskCreate( prvTimeManageTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) TIME_MANAGE_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					TIME_MANAGE_PRIORITY, 			/* The priority assigned to the task. */
					&temp_HANDLE );								/* The task handle is not required, so NULL is passed. */
	return temp_HANDLE;
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

void broadcast_minute(void)
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
void update_absolute_time(void)
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
	
	CURRENT_TIME = ((uint32_t)ABSOLUTE_DAY) << 24;
	CURRENT_TIME += ((uint32_t)CURRENT_HOUR) << 16;
	CURRENT_TIME += ((uint32_t)CURRENT_MINUTE) << 8;
	CURRENT_TIME += (uint32_t)CURRENT_SECOND;
	
	spimem_write(TIME_BASE, absolute_time_arr, 4);	// Writes the absolute time to SPI memory.
	return;
}

void report_time(void)
{
	clear_current_command();
	current_command[9] = TIME_REPORT;
	current_command[3] = absolute_time_arr[3];
	current_command[2] = absolute_time_arr[2];
	current_command[1] = absolute_time_arr[1];
	current_command[0] = absolute_time_arr[0];
	
	xQueueSendToBackTask(TIME_TASK_ID, 1, time_to_obc_fifo, current_command, (TickType_t)1);
	minute_count = 0;
	return;
}

// Execute commands that are sent from the obc_packet_router
static void exec_commands(void)
{
	uint16_t packet_id, psc;
	if(xQueueReceiveTask(TIME_TASK_ID, 0, obc_to_time_fifo, current_command, (TickType_t)10) == pdTRUE)
	{
		packet_id = ((uint16_t)current_command[8]) << 8;
		packet_id += (uint16_t)current_command[7];
		psc = ((uint16_t)current_command[6]) << 8;
		psc += (uint16_t)current_command[5];
		report_timeout = current_command[0];			
		send_tc_execution_verify(1, packet_id, psc);
	}
	if(xQueueReceive(sched_to_time_fifo, current_command, (TickType_t)1))
	{
		report_timeout = current_command[0];
		send_tc_execution_verify(1, 0, 0);
	}
	return;
}

// status = 0x01 for success, 0xFF for failure.
static void send_tc_execution_verify(uint8_t status, uint16_t packet_id, uint16_t psc)
{
	clear_current_command();
	current_command[9] = TASK_TO_OPR_TCV;		// Request a TC verification
	current_command[8] = status;
	current_command[7] = TIME_TASK_ID;			// APID of this task
	current_command[6] = ((uint8_t)packet_id) >> 8;
	current_command[5] = (uint8_t)packet_id;
	current_command[4] = ((uint8_t)psc) >> 8;
	current_command[3] = (uint8_t)psc;
	return;
}

// This function will kill this task.
// If it is being called by this task 0 is passed, otherwise it is probably the FDIR task and 1 should be passed.
void time_manage_kill(uint8_t killer)
{
	// Free the memory that this task allocated.
	//vPortFree(current_command);
	//vPortFree(minute_count);
	//vPortFree(report_timeout);
	// Kill the task.
	if(killer)
		vTaskDelete(time_manage_HANDLE);
	else
		vTaskDelete(NULL);
	return;
}
