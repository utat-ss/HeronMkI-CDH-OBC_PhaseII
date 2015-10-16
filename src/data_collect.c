/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		data_collect.c
	*
	*	PURPOSE:		
	*	This file is to be used to encapsulate all functions and tasks related to data collection.
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
	*	03/03/2015		Created.
	*
	*	08/07/2015		Added the high_command_generator() function which generates the upper four
	*					bytes which are required in our new CAN organizational structure.
	*
	*	DESCRIPTION:	
	*
	*	This file is being used to collect data from subsystem microcontrollers by sending out those
	*	requests over CAN periodically with a task.
	*
	*	The task will send out a CAN request asking a specific subsystem for their data.
	*
	*	It will then delay 15 clock cycles and send the message again.
	*
	*	After sending a remote request, a reply message with data should then be received.
	*	
 */

/* Standard includes.										*/
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
/* Global Variable includes */
#include "global_var.h"
/*-----------------------------------------------------------*/

/* Priorities at which the tasks are created. */
#define Data_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )		// Lower the # means lower the priority

/* Values passed to the two tasks just to check the task parameter functionality. */
#define DATA_PARAMETER			( 0xABCD )

/* Function Prototypes										*/
static void prvDataTask( void *pvParameters );
void data_test(void);
/*-----------------------------------------------------------*/

/************************************************************************/
/* DATA_TEST	 														*/
/* @Purpose: This function is simply used to create the data task below	*/
/* in main.c															*/
/************************************************************************/
void data_test( void )
{
	/* Start the two tasks as described in the comments at the top of this
	file. */
	xTaskCreate( prvDataTask,					/* The function that implements the task. */
				"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
				configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
				( void * ) DATA_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
				Data_TASK_PRIORITY, 			/* The priority assigned to the task. */
				NULL );								/* The task handle is not required, so NULL is passed. */
	return;
}

/************************************************************************/
/*				PRVDATATASK												*/
/* @purpose: This task is used to demonstrate how sensor data or the	*/
/* like can be															*/
/* requested from an SSM and then sent to the OBC.						*/
/************************************************************************/
static void prvDataTask( void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == DATA_PARAMETER );
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 1;	//Number entered here corresponds to the number of ticks we should wait.
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
	
	uint32_t low, high, ID, PRIORITY, x, i;
	
	ID = SUB0_ID0;
	PRIORITY = DATA_PRIO;
	
	/* @non-terminating@ */	
	for( ;; )
	{
		low = DATA_REQUEST;
		high = high_command_generator(OBC_ID, MT_COM, REQ_DATA);
		
		ID = SUB1_ID0;
		x = send_can_command(low, high, ID, PRIORITY);				// Request data from COMS.
			
		ID = SUB0_ID0;
		x = send_can_command(low, high, ID, PRIORITY);				// Request data from EPS.

		ID = SUB2_ID0;
		x = send_can_command(low, high, ID, PRIORITY);				// Request data from PAY.						

		if(glob_drf)		// data reception flag;
		{
			x = read_can_data(&high, &low, 1234);
			// ** Modify this code so that it checks the small types first.
			if(x)
			{
				glob_stored_data[1] = high;
				glob_stored_data[0] = low;
				glob_drf = 0;
			}
		}
		
		if(glob_comsf)
		{
			x = read_can_msg(&high, &low, 1234);

			if(x)
			{
				glob_stored_message[1] = high;
				glob_stored_message[0] = low;
				glob_comsf = 0;
			}
		}
	}
	
	//xLastWakeTime = xTaskGetTickCount();						// Delay for 100 ticks.
	//vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
}

