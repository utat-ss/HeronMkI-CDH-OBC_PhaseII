/*
Author: Keenan Burnett
***********************************************************************
* FILE NAME: payload.c
*
* PURPOSE:
* This file is to be used for the high-level payload-related software on the satellite.
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
* Remember that configTICK_RATE_HZ in FreeRTOSConfig.h is currently set to 10 Hz and
* so when that is set to a new value, the amount of ticks in between housekeeping will
* have to be adjusted.
*
* REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
* New tasks should be written to use as much of CMSIS as possible. The ASF and
* FreeRTOS API libraries should also be used whenever possible to make the program
* more portable.
*
* DEVELOPMENT HISTORY:
* 07/06/2015 	Created.
*
* DESCRIPTION:
*
* Current this file is set up as a template and shall be used by members of the payload subsystem
* for their own development purposes.
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
#define Payload_PRIORITY	( tskIDLE_PRIORITY + 1 ) // Lower the # means lower the priority
/* Values passed to the two tasks just to check the task parameter
functionality. */
#define PAYLOAD_PARAMETER	( 0xABCD )
/*-----------------------------------------------------------*/

/* Function Prototypes */
static void prvPayloadTask( void *pvParameters );
void payload(void);

/* Function Definitions */

/************************************************************************/
/* COMS Function 														*/
/* 																		*/
/* This fuction is to be used in main.c to create the coms task.		*/
/*																		*/
/************************************************************************/
void payload( void )
{
/* Start the two tasks as described in the comments at the top of this
file. */
xTaskCreate( prvPayloadTask, /* The function that implements the task. */
"ON", /* The text name assigned to the task - for debug only as it is not used by the kernel. */
configMINIMAL_STACK_SIZE, /* The size of the stack to allocate to the task. */
( void * ) PAYLOAD_PARAMETER, /* The parameter passed to the task - just to check the functionality. */
Payload_PRIORITY, /* The priority assigned to the task. */
NULL ); /* The task handle is not required, so NULL is passed. */

/* If all is well, the scheduler will now be running, and the following
line will never be reached. If the following line does execute, then
there was insufficient FreeRTOS heap memory available for the idle and/or
timer tasks to be created. See the memory management section on the
FreeRTOS web site for more details. */
return;
}

/************************************************************************/
/* COMS TASK */
/* This task encapsulates the high-level software functionality of the	*/
/* communication subsystem on this satellite.							*/
/*																		*/
/************************************************************************/
static void prvComsTask(void *pvParameters )
{
configASSERT( ( ( unsigned long ) pvParameters ) == PAYLOAD_PARAMETER );
TickType_t xLastWakeTime;
const TickType_t xTimeToWait = 15; // Number entered here corresponds to the number of ticks we should wait.
/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/

/* Declare Variables Here */


/* @non-terminating@ */	
for( ;; )
{
	// Write your application here.
}
}

