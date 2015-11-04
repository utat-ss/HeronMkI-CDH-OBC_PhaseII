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
	*			This service needs to store housekeeping / diagnostic "definitions"
	*			which define how hk or diag is put together before being downlinked.
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
	*	11/04/2015		I am adding in functionality so that we periodically send a housekeeping packet
	*					to obc_packet_router through hk_to_t_fifo.
	*					HK "packets" are just the data field of the PUS packet to be sent and are hence
	*					a maximum of 128 B. They can be larger, but then they must be split into multiple
	*					packets.
	*
	*	DESCRIPTION:
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
 * Function Prototypes.
 */
static void prvHouseKeepTask( void *pvParameters );
void housekeep(void);
static void clear_current_hk(void);

/* Global Variables for Housekeeping */
static uint8_t current_hk[DATA_LENGTH];				// Used to store the next housekeeping packet we would like to downlink.
static uint32_t new_kh_msg_high, new_hk_msg_low;
static uint8_t scheduled_collectionf;

/************************************************************************/
/* HOUSEKEEPING (Function) 												*/
/* @Purpose: This function is used to create the housekeeping task.		*/
/************************************************************************/
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
/* This task is used to periodically gather housekeeping information	*/
/* and store it in external memory until it is ready to be downlinked.  */
/************************************************************************/
static void prvHouseKeepTask(void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == HK_PARAMETER );
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 100;	// Number entered here corresponds to the number of ticks we should wait.
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
	int x;
	uint8_t i;
	uint8_t passkey = 1234, addr = 0x80;
	new_kh_msg_high = 0;
	new_hk_msg_low = 0;
	scheduled_collectionf = 1;
	clear_current_hk();
		
	/* @non-terminating@ */	
	for( ;; )
	{
		if(scheduled_collectionf)
		{
			x = request_housekeeping(EPS_ID);								// Request housekeeping from COMS.
			x = request_housekeeping(COMS_ID);								// Request housekeeping from EPS.
			x = request_housekeeping(PAY_ID);								// Request housekeeping from PAY.

			xTimeToWait = 100;												// Give the SSMs >100ms to transmit their housekeeping
			xLastWakeTime = xTaskGetTickCount();
			vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
			scheduled_collectionf = 0;
		}

		
		x = read_can_hk(&new_kh_msg_high, &new_hk_msg_low, passkey);
		if(x > 0)
			store_housekeeping();
		
		
		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
		
		passkey ++;
	}
}
/*-----------------------------------------------------------*/

// Define Static helper functions below.

static void clear_current_hk(void)
{
	uint8_t i;
	for(i = 0; i < DATA_LENGTH; i++)
	{
		current_hk[i] = 0;
	}
	return;
}