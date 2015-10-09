/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		housekeep.c
	*
	*	PURPOSE:		
	*	This file is to be used to create the housekeeping task needed to monitor
	*	housekeeping information on the satellite.
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
	*	02/18/2015		Created.
	*
	*	08/09/2015		Added the function read_from_SSM() to this tasks's duties as a demonstration
	*					of our reprogramming capabilities.
	*
	*	DESCRIPTION:	
	*
	*	This file is being used to test Housekeeping Commands between the OBC and a subsystem micro. 
	*	This file is used to encapsulate a test function called houskeep_test2(), which will create a 
	*	task that will send housekeeping request as a can message from CAN0 MB7 to MOb0 on the 
	*	ATMEGA32M1 supported by the STK600.
	*
	*	It will then delay 15 clock cycles and send the message again.
	*
	*	After sending a remote request, a reply message should then be received.
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

/* Priorities at which the tasks are created. */
#define Housekeep_PRIORITY		( tskIDLE_PRIORITY + 1 )		// Lower the # means lower the priority

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define HK_PARAMETER			( 0xABCD )

/*-----------------------------------------------------------*/

/*
 * Functions Prototypes.
 */
static void prvHouseKeepTask( void *pvParameters );
void housekeep(void);

/************************************************************************/
/*			2ND	TEST FUNCTION FOR HOUSEKEEPING                          */
/************************************************************************/
/**
 * \brief Tests the housekeeping task.
 */
void housekeep( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvHouseKeepTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) HK_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					Housekeep_PRIORITY, 			/* The priority assigned to the task. */
					NULL );								/* The task handle is not required, so NULL is passed. */
					
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	return;
}

/************************************************************************/
/*				HOUSEKEEPING TASK		                                */
/*	The sole purpose of this task is to send a housekeeping request to	*/
/*	MOB5 on the ATMEGA32M1 which is being supported by the STK600.		*/
/************************************************************************/
static void prvHouseKeepTask(void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == HK_PARAMETER );
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 100;	// Number entered here corresponds to the number of ticks we should wait.
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
	
	uint32_t ID, x;
	uint8_t ret_val, passkey = 0, addr = 0x80;
		
	/* @non-terminating@ */	
	for( ;; )
	{
		ID = SUB1_ID5;
		x = request_housekeeping(ID);								// Request housekeeping from COMS.
			
		xLastWakeTime = xTaskGetTickCount();						// Delay for 1 tick.
		vTaskDelayUntil(&xLastWakeTime, (TickType_t) 1);
			
		ID = SUB0_ID5;
		x = request_housekeeping(ID);								// Request housekeeping from EPS.
			
		xLastWakeTime = xTaskGetTickCount();						// Delay for 1 tick.
		vTaskDelayUntil(&xLastWakeTime, (TickType_t) 1);
		
		ID = SUB2_ID5;
		x = request_housekeeping(ID);								// Request housekeeping from PAY.
		//ret_val = read_from_SSM(HK_TASK_ID, SUB0_ID0, passkey, addr);
		
		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
		
		passkey ++;
	}
}
/*-----------------------------------------------------------*/

