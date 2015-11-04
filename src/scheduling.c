/*
Author: Keenan Burnett
***********************************************************************
* FILE NAME: obc_packet_router.c
*
* PURPOSE:
* This file is to be used to house the scheduling task and related functions.
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
* 11/01/2015		Created.
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
#define SCHEDULING_PRIORITY		( tskIDLE_PRIORITY + 4 )

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define SCHEDULING_PARAMETER			( 0xABCD )

/*
 * Functions Prototypes.
 */
static void prvSchedulingTask( void *pvParameters );
void scheduling(void);

/************************************************************************/
/* SCHEDULING (Function)												*/
/* @Purpose: This function is used to create the scheduling task.		*/
/************************************************************************/
void scheduling( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvSchedulingTask(),					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) SCHEDULING_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					SCHEDULING_PRIORITY, 			/* The priority assigned to the task. */
					NULL );								/* The task handle is not required, so NULL is passed. */
	return;
}
/*-----------------------------------------------------------*/

/************************************************************************/
/*				SCHEDULING TASK			                                */
/*	This task receives scheduling requests from obc_packet_router and	*/
/*  other tasks/ SSMs and then places them in SPI Memory.				*/
/* This task also periodically checks if a scheduled command needs to	*/
/* be performed and subsequently carries out required commands.			*/
/************************************************************************/
static void prvSchedulingTask( void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == SCHEDULING_PARAMETER );
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 10;	// Number entered here corresponds to the number of ticks we should wait.

	/* @non-terminating@ */	
	for( ;; )
	{
		// Write task here.
	}
}
/*-----------------------------------------------------------*/

// static helper functions may be defined below.
