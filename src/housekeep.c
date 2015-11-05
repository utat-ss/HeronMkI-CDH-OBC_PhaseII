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
	*			When housekeeping or diagnostics are immediately requested, new values HAVE to be sampled.
	*
	*			Consider thinking about how to guarantee that every time housekeeping is downlinked those
	*			values are fresh.
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
static uint8_t current_hk[DATA_LENGTH];			// Used to store the next housekeeping packet we would like to downlink.
static uint8_t hk_definition[PACKET_LENGTH];	// Used to store the current housekeeping format definition.
static uint8_t hk_format;						// Unique identifier for the housekeeping format definition.
static uint8_t current_eps_hk[DATA_LENGTH / 4], current_coms_hk[DATA_LENGTH / 4], current_pay_hk[DATA_LENGTH / 4], current_obc_hk[DATA_LENGTH / 4];
static uint32_t new_kh_msg_high, new_hk_msg_low;
static uint8_t scheduled_collectionf, current_hk_fullf;

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
	current_hk_fullf = 0;
	hk_format = 0;					// Default definition version.
	clear_current_hk();
	set_default_definition();
		
	/* @non-terminating@ */	
	for( ;; )
	{
		if(scheduled_collectionf)
		{
			request_housekeeping_all();
			store_housekeeping();
		}
		
		if(!scheduled_collectionf)
		{
			xqueue
		}

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

static int request_housekeeping_all(void)
{			
	if(request_housekeeping(EPS_ID) > 1)							// Request housekeeping from COMS.
		return -1;
	if(request_housekeeping(COMS_ID > 1)							// Request housekeeping from EPS.
		return -1;
	if(request_housekeeping(PAY_ID) > 1)							// Request housekeeping from PAY.
		return -1;

	xTimeToWait = 100;												// Give the SSMs >100ms to transmit their housekeeping
	xLastWakeTime = xTaskGetTickCount();
	vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
	scheduled_collectionf = 0;
	return 1;
}


// This function does not erase old values, it only updates them.
static int store_housekeeping(void)
{
	uint8_t sender = 0xFF;
	uint8_t parameter_name = 0, i;
	if(current_hk_fullf)
		return -1;
	
	while(read_can_hk(&new_kh_msg_high, &new_hk_msg_low, passkey) == 1)
	{
		sender = (new_kh_msg_high & 0xF0000000) >> 28;			// Can be EPS_ID/COMS_ID/PAY_ID/OBC_ID
		parameter_name = (new_kh_msg_high & 0x0000FF00) >> 8;	// Name of the parameter for housekeeping (either sensor or variable).
		for(i = 0; i < DATA_LENGTH; i += 2)
		{
			if(hk_definition[i] == parameter_name)
				current_hk[i] = (uint8_t)(new_hk_msg_low & 0x000000FF);
				current_hk[i + 1] = (uint8_t)((new_hk_msg_low & 0x0000FF00) >> 8); 
		}
		taskYIELD();		// Allows for more messages to come in.
	}
	
	// Collect variable values
	
	// Make sure that all values have been updated.
		// Reacquire values if they haven't been.
	
	current_hk_fullf = 1;
	return 1;
}

// This function sets the default definition that is used upon
// startup for sending housekeeping packet.
static void set_default_definition(void)
{
	uint8_t i;
	
	for(i = 0; i < DATA_LENGTH; i++)
	{
		hk_definition[i] = 0;
	}
	hk_definition[75] = PANELX_V;
	hk_definition[74] = PANELX_V;
	hk_definition[73] = PANELX_I;
	hk_definition[72] = PANELX_I;
	hk_definition[71] = PANELY_V;
	hk_definition[70] = PANELY_V;
	hk_definition[69] = PANELY_I;
	hk_definition[68] = PANELY_I;
	hk_definition[67] = BATTM_V;
	hk_definition[66] = BATTM_V;
	hk_definition[65] = BATT_V;
	hk_definition[64] = BATT_V;
	hk_definition[63] = BATTIN_I;
	hk_definition[62] = BATTIN_I;
	hk_definition[61] = BATTOUT_I;
	hk_definition[60] = BATTOUT_I;
	hk_definition[59] = BATT_TEMP;
	hk_definition[58] = BATT_TEMP;	//
	hk_definition[57] = EPS_TEMP;
	hk_definition[56] = EPS_TEMP;	//
	hk_definition[55] = COMS_V;
	hk_definition[54] = COMS_V;
	hk_definition[53] = COMS_I;
	hk_definition[52] = COMS_I;
	hk_definition[51] = PAY_V;
	hk_definition[50] = PAY_V;
	hk_definition[49] = PAY_I;
	hk_definition[48] = PAY_I;
	hk_definition[47] = OBC_V;
	hk_definition[46] = OBC_V;
	hk_definition[45] = OBC_I;
	hk_definition[44] = OBC_I;
	hk_definition[43] = BATT_I;
	hk_definition[42] = BATT_I;
	hk_definition[41] = EPS_MODE;	//
	hk_definition[40] = EPS_MODE;
	hk_definition[39] = COMS_TEMP;	//
	hk_definition[38] = COMS_TEMP;
	hk_definition[37] = COMS_MODE;	//
	hk_definition[36] = COMS_MODE;
	hk_definition[35] = PAY_TEMP0;
	hk_definition[34] = PAY_TEMP0;
	hk_definition[33] = PAY_TEMP1;
	hk_definition[32] = PAY_TEMP1;
	hk_definition[31] = PAY_TEMP2;
	hk_definition[30] = PAY_TEMP2;
	hk_definition[29] = PAY_TEMP3;
	hk_definition[28] = PAY_TEMP3;
	hk_definition[27] = PAY_TEMP4;
	hk_definition[26] = PAY_TEMP4;
	hk_definition[25] = PAY_HUM;
	hk_definition[24] = PAY_HUM;
	hk_definition[23] = PAY_PRESS;
	hk_definition[22] = PAY_PRESS;
	hk_definition[21] = PAY_MODE;
	hk_definition[20] = PAY_MODE;
	hk_definition[19] = PAY_STATE;
	hk_definition[18] = PAY_STATE;
	hk_definition[17] = OBC_TEMP;
	hk_definition[16] = OBC_TEMP;
	hk_definition[15] = OBC_MODE;
	hk_definition[14] = OBC_MODE;
	hk_definition[13] = ABS_TIME_D;
	hk_definition[12] = ABS_TIME_D;
	hk_definition[11] = ABS_TIME_H;
	hk_definition[10] = ABS_TIME_H;
	hk_definition[9] = ABS_TIME_M;
	hk_definition[8] = ABS_TIME_M;
	hk_definition[7] = ABS_TIME_S;
	hk_definition[6] = ABS_TIME_S;
	hk_definition[5] = SPI_CHIP_1;
	hk_definition[4] = SPI_CHIP_1;
	hk_definition[3] = SPI_CHIP_2;
	hk_definition[2] = SPI_CHIP_2;
	hk_definition[1] = SPI_CHIP_3;
	hk_definition[0] = SPI_CHIP_3;
	return;
}

