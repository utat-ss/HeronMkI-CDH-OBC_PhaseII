/*
Author: Keenan Burnett
***********************************************************************
* FILE NAME: fdir.c
*
* PURPOSE:
* This file is to be used to house the FDIR task and related functions.
*	FDIR == "Failure Detection, Isolation, and Recovery"
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
* TC = Telecommand (things sent up to the satellite)
* TM = Telemetry   (things sent down to ground)
*
* REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
* DEVELOPMENT HISTORY:
* 11/10/2015		Created.
*
* 12/09/2015		I added in check_error() and decode_error() and am starting to fill in their blank
*					parts with resolution sequences from FDIR.docx in the Owncloud.
*
* DESCRIPTION:
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
/*		RTC includes.	*/
#include "rtc.h"
/*		CAN includes	*/
#include "can_func.h"
/*		Global variables used in entire project */
#include "global_var.h"
/*		SPI includes	*/
#include "spimem.h"
/*	All things Error-handling related */
#include "error_handling.h"

/* Priorities at which the tasks are created. */
#define FDIR_PRIORITY		( tskIDLE_PRIORITY + 5 )

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define FDIR_PARAMETER			( 0xABCD )

/* Functions Prototypes. */
static void prvFDIRTask( void *pvParameters );
TaskHandle_t fdir(void);
static void check_error(void);
static void decode_error(uint32_t error, uint8_t severity);
static void clear_current_command(void);

/* External functions used to create and encapsulate different tasks*/
extern TaskHandle_t housekeep(void);
extern TaskHandle_t time_manage(void);
extern TaskHandle_t eps(void);
extern TaskHandle_t coms(void);
extern TaskHandle_t payload(void);
extern TaskHandle_t memory_manage(void);
extern TaskHandle_t wdt_reset(void);
extern TaskHandle_t obc_packet_router(void);
extern TaskHandle_t scheduling(void);

/* External functions used to kill different tasks	*/
extern void housekeep_suicide(void);

/* Global Variables for Housekeeping */
static uint8_t current_command[DATA_LENGTH + 10];	// Used to store commands which are sent from the OBC_PACKET_ROUTER.
/************************************************************************/
/* FDIR (Function)														*/
/* @Purpose: This function is used to create the fdir task.				*/
/************************************************************************/
TaskHandle_t fdir( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		TaskHandle_t temp_HANDLE;
		xTaskCreate( prvFDIRTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) FDIR_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					FDIR_PRIORITY, 			/* The priority assigned to the task. */
					&temp_HANDLE );								/* The task handle is not required, so NULL is passed. */
	return temp_HANDLE;
}
/*-----------------------------------------------------------*/

/************************************************************************/
/*				FDIR TASK												*/
/*	This task receives commands from obc_packet_router as well as		*/
/* failure messages from other tasks. It then proceeds to attempt to	*/
/* resolve the issue as best it can.									*/
/************************************************************************/
static void prvFDIRTask( void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == FDIR_PARAMETER );
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 10;	// Number entered here corresponds to the number of ticks we should wait.

	/* @non-terminating@ */	
	for( ;; )
	{
		check_error();
	}
}
/*-----------------------------------------------------------*/

// Static functions defined below.

// This function shall be used to check if there is any required action for the FDIR task to deal with.
static void check_error(void)
{
	uint32_t error = 0;
	// Check if there is a high-severity error to deal with.
	if (xSemaphoreTake(Highsev_Mutex, (TickType_t)1) == pdTRUE)
	{
		if( xQueueReceive(high_sev_to_fdir_fifo, current_command, (TickType_t)1) == pdTRUE)
		{
			error = ((uint32_t)(current_command[151])) << 24;
			error += ((uint32_t)current_command[150]) << 16;
			error += ((uint32_t)current_command[149]) << 8;
			error += ((uint32_t)current_command[148]);
			decode_error(error, 1);
		}
		xSemaphoreGive(Highsev_Mutex);
	}
	// Check if there is a low-severity error to deal with.
	if (xSemaphoreTake(Lowsev_Mutex, (TickType_t)1) == pdTRUE)
	{
		if( xQueueReceive(low_sev_to_fdir_fifo, current_command, (TickType_t)1) == pdTRUE)
		{
			error = ((uint32_t)(current_command[151])) << 24;
			error += ((uint32_t)current_command[150]) << 16;
			error += ((uint32_t)current_command[149]) << 8;
			error += ((uint32_t)current_command[148]);
			decode_error(error, 2);
		}
		xSemaphoreGive(Highsev_Mutex);	
	}

	return;
}

static void decode_error(uint32_t error, uint8_t severity)
{
	// This is where the resolution sequences are going to go.
}


/************************************************************************/
/* CLEAR_CURRENT_COMMAND												*/
/* @Purpose: clears the array current_command[]							*/
/************************************************************************/
static void clear_current_command(void)
{
	uint8_t i;
	for(i = 0; i < (DATA_LENGTH + 10); i++)
	{
		current_command[i] = 0;
	}
	return;
}

int restart_task(xTaskHandle task_to_restart)
{
	// Delete the given task.
	
	
	
	// Now recreate it.
}