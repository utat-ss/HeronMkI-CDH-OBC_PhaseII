/*
Author: Keenan Burnett
***********************************************************************
* FILE NAME: coms.c
*
* PURPOSE:
* This file is to be used for the high-level communications-related software on the satellite.
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
* 07/06/2015 	K: Created.
*
* 10/09/2015	K: Updated comments and a few lines to make things neater.
*
* DESCRIPTION:
*
* Current this file is set up as a template and shall be used by members of the coms subsystem
* for their own development purposes.
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
/* Priorities at which the tasks are created. */
#define Coms_PRIORITY	( tskIDLE_PRIORITY + 1 ) // Lower the # means lower the priority
/* Values passed to the two tasks just to check the task parameter
functionality. */
#define COMS_PARAMETER	( 0xABCD )

#define PACKET_LENGTH	143
/*-----------------------------------------------------------*/

/* Function Prototypes										 */
static void prvComsTask( void *pvParameters );
void coms(void);
/*-----------------------------------------------------------*/

/* Global variables											 */
uint8_t current_tc[PACKET_LENGTH], current_tm[PACKET_LENGTH];
uint8_t version;
uint8_t type, data_header, flag, apid, sequence_flags, sequence_count;
uint16_t abs_time;

/************************************************************************/
/* COMS (Function) 														*/
/* @Purpose: This function is simply used to create the COMS task below	*/
/* in main.c															*/
/************************************************************************/
void coms( void )
{
	/* Start the two tasks as described in the comments at the top of this
	file. */
	xTaskCreate( prvComsTask, /* The function that implements the task. */
		"ON", /* The text name assigned to the task - for debug only as it is not used by the kernel. */
		configMINIMAL_STACK_SIZE, /* The size of the stack to allocate to the task. */
		( void * ) COMS_PARAMETER, /* The parameter passed to the task - just to check the functionality. */
		Coms_PRIORITY, /* The priority assigned to the task. */
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
	configASSERT( ( ( unsigned long ) pvParameters ) == COMS_PARAMETER );
	TickType_t xLastWakeTime;
	const TickType_t xTimeToWait = 15; // Number entered here corresponds to the number of ticks we should wait.
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/

	/* Declare Variables Here */
	sequence_count = 0;


	/* @non-terminating@ */	
	for( ;; )
	{
		// Write your application here.
		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);		// This is what delays your task if you need to yield. Consult CDH before editing.
	}
}

// Static helper functions may be defined below.


// Remember to keep track of "absolute time"

// *data should be a pointer to a 128-byte buffer which shall contain the contents of the message to be sent.

static int packetize_send_telemetry(uint8_t sender, uint8_t dest, uint8_t service_type, uint8_t service_sub_type, uint8_t packet_sub, uint16_t num_packets, uint8_t* data)
{
	uint16_t i, j;
	
	version = 0;		// First 3 bits of the packet ID. (0 is default)
	type = 0;			// Distinguishes TC and TM packets, TC == 1.
	data_header = 1;	// Include the data field header in the PUS packet.
	apid = sender;
	
	if(num_packets > 1)
		sequence_flags = 0x1;	// Indicates that this is the first packet in a series of packets.
	else
		sequence_flags = 0x3;	// Indicates that this is a standalone packet.
		
	// Packet Header	
	current_tm[142] = ((version & 0x07) << 5) & ((type & 0x01);
	current_tm[141] = apid;
	current_tm[140] = (sequence_flags & 0x03) << 6;
	current_tm[139] = sequence_count;
	current_tm[138] = (uint8_t)((num_packets & 0xFF00) >> 8);
	current_tm[137]	 = (uint8_t)(num_packets & 0x00FF);
	// Data Field Header
	current_tm[136] = (version & 0x07) << 4;
	current_tm[135] = service_type;
	current_tm[134] = service_sub_type;
	current_tm[133] = packet_sub;
	current_tm[132] = dest;
	current_tm[131] = (uint8_t)((abs_time & 0xFF00) >> 8);
	current_tm[130] = (uint8_t)(abs_time & 0x00FF);
	
	// The Packet Error Control (PEC) is usually put at the end,
	// this is usually a CRC on the rest of the packet.
	
	if(num_packets == 1)
	{
		sequence_count++;
		current_tm[139] = sequence_count;
		
		for(j = 2; j < (PACKET_LENGTH - 13); j++)
		{
			current_tm[j] = *(data + (j - 2));
		}
		
		// Run CRC function here to fill in current_tm[1,0]
		
		if( send_pus_packet() < 0)
			return 0;
			
		return 1;
	}
	
	for(i = 0; i < num_packets; i++)
	{
		sequence_count++;
		current_tm[139] = sequence_count;
		
		if(i > 1)
			sequence_flags = 0x0;			// Continuation packet
		if(i == (num_packets - 1))
			sequence_flags = 0x2;			// Last packet
		current_tm[140] = (sequence_flags & 0x03) << 6;
		
		for(j = 0; j < (PACKET_LENGTH - 13); j++)
		{
			current_tm[j] = *(data + (j - 2) + (i * 128));
		}
		
		// Run CRC function here to fill in current_tm[1,0]
				
		if(send_pus_packet() < 0)
			return i;
	}
	
	return num_packets;
}

static int receive_telemetry(uint8_t sender, uint8_t dest, uint8_t packet_num, uint8_t* data)
{
	
}