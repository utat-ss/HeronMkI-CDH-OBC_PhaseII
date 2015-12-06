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

#include "can_func.h"

#include "global_var.h"

/* Priorities at which the tasks are created. */
#define FDIR_PRIORITY		( tskIDLE_PRIORITY + 5 )

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define FDIR_PARAMETER			( 0xABCD )

/*
 * Functions Prototypes.
 */
static void prvFDIRTask( void *pvParameters );
TaskHandle_t fdir(void);

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
		//
	}
}
/*-----------------------------------------------------------*/

// Static functions defined below.