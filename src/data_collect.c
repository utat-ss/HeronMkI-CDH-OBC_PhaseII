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
	*	Remember that configTICK_RATE_HZ in FreeRTOSConfig.h is currently set to 10 Hz and
	*	so when that is set to a new value, the amount of ticks in between housekeeping will
	*	have to be adjusted.
	*	
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:			
	*	New tasks should be written to use as much of CMSIS as possible. The ASF and 
	*	FreeRTOS API libraries should also be used whenever possible to make the program
	*	more portable.

	*	DEVELOPMENT HISTORY:		
	*	03/03/2015		Created.
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

/* Global Variable includes */
#include "global_var.h"


/* Priorities at which the tasks are created. */
#define Data_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )		// Lower the # means lower the priority

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define DATA_PARAMETER			( 0xABCD )

/*-----------------------------------------------------------*/

/*
 * Functions Prototypes.
 */
static void prvDataTask( void *pvParameters );
void data_test(void);

/************************************************************************/
/*			TEST FUNCTION FOR COMMANDS TO THE STK600                    */
/************************************************************************/
/**
 * \brief Tests the housekeeping task.
 */
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
/*-----------------------------------------------------------*/

/************************************************************************/
/*				COMMAND TASK		                                    */
/*	The sole purpose of this task is to send a single CAN message over	*/
/*	and over in order to test the STK600's CAN reception.				*/
/************************************************************************/
/**
 * \brief Performs the housekeeping task.
 * @param *pvParameters:
 */
static void prvDataTask( void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == DATA_PARAMETER );
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 15;	//Number entered here corresponds to the number of ticks we should wait.
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
	
	uint32_t low, high, ID, PRIORITY, x, i;
	
	uint32_t* message, mem_ptr;
	
	low = DATA_REQUEST;
	high = DATA_REQUEST;
	ID = SUB0_ID0;
	PRIORITY = DATA_PRIO;
	
	/* @non-terminating@ */	
	for( ;; )
	{
		//xSemaphoreTake(Can1_Mutex, 2);							// Acquire CAN1 Mutex
		x = send_can_command(low, high, ID, PRIORITY);				//This is the CAN API function I have written for us to use.
		//xSemaphoreGive(Can1_Mutex);								// Release CAN1 Mutex
		
		xLastWakeTime = xTaskGetTickCount();						// Delay for 15 clock cycles.
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);

		//xSemaphoreTake(Can1_Mutex, 2);							// Acquire CAN1 Mutex
		if(glob_drf)		// data reception flag;
		{
			x = read_can_data(&high, &low, 1234);

			if(x)
			{
				glob_stored_data[1] = *high;
				glob_stored_data[0] = *low;
				glob_drf = 0;
			}
		}
		
		if(glob_comf)
		{
			x = read_can_msg(&high, &low, 1234);

			if(x)
			{
				glob_stored_message[1] = *high;
				glob_stored_message[0] = *low;
				glob_comf = 0;
			}
		}
		//xSemaphoreGive(Can1_Mutex);								// Release CAN1 Mutex
	}
}
/*-----------------------------------------------------------*/

