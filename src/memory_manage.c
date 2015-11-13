/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		memory_manage.c
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
	*			I need to write a function that goes through the SPI memory and executes Fletcher64 on it.
	*	
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:			
	*	New tasks should be written to use as much of CMSIS as possible. The ASF and 
	*	FreeRTOS API libraries should also be used whenever possible to make the program
	*	more portable.

	*	DEVELOPMENT HISTORY:		
	*	10/15/2015		Created.
	*
	*	11/07/2015		Changed name from memory_wash to memory_manage to reflect the change
	*					in what this task is supposed to be doing.
	*
	*					I am adding the bulk of the required PUS service functionality with the use of exec_commands().
	*
	*	DESCRIPTION:	
	*
	*	This task is meant to fulfill the PUS Memory Management Service.
	*
	*	This task shall also periodically "wash" the external SPI Memory by checking for bit flips and
	*	subsequently correcting them.
	*
	*	In order to do this, we intend to have 3 external SPI Memory chips and use a "voting"
	*	method in order to correct bit flips on the devices.
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

/* Common includes. */
#include "partest.h"

#include "spimem.h"

#include "global_var.h"

#include "checksum.h"

/* Priorities at which the tasks are created. */
#define MEMORY_MANAGE_PRIORITY	( tskIDLE_PRIORITY + 4 )		// Lower the # means lower the priority

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define MM_PARAMETER			( 0xABCD )

/* Definitions to clarify which service subtypes represent what	*/
/* Memory Management					*/
#define MEMORY_LOAD_ABS					2
#define DUMP_REQUEST_ABS				5
#define MEMORY_DUMP_ABS					6
#define CHECK_MEM_REQUEST				9
#define MEMORY_CHECK_ABS				10

/*-----------------------------------------------------------*/

static uint8_t page_buff1[256], page_buff2[256], page_buff3[256]; 
/* Functions Prototypes. */
static void prvMemoryManageTask( void *pvParameters );
void menory_manage(void);
static void memory_wash(void);
static void exec_commands(void);
static void clear_current_command(void);
static void send_tc_execution_verify(uint8_t status, uint16_t packet_id, uint16_t pcs);
static void send_event_report(uint8_t severity, uint8_t report_id, uint8_t param1, uint8_t param0);

/* Local variables for memory management */
static uint8_t minute_count;
static uint8_t current_command[DATA_LENGTH + 10];
static const TickType_t xTimeToWait = 60000;	// Number entered here corresponds to the number of ticks we should wait (1 minute)

/************************************************************************/
/* MEMORY_WASH (Function)												*/
/* @Purpose: This function is used to create the memory washing task.	*/
/************************************************************************/
void memory_manage( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvMemoryManageTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) MM_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					MEMORY_MANAGE_PRIORITY, 			/* The priority assigned to the task. */
					NULL );								/* The task handle is not required, so NULL is passed. */
					
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	return;
}

/************************************************************************/
/*				MEMORY WASH (TASK)		                                */
/*	This task will periodically take control of the SPI Memory Chips and*/
/* verify that there are no bit flips within them by way of a voting    */
/* algorithm between 3 separate chips storing redundant data.			*/
/************************************************************************/
static void prvMemoryManageTask(void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == MM_PARAMETER );
	minute_count = 0;
	clear_current_command();
	/* @non-terminating@ */	
	for( ;; )
	{
		minute_count++;
		if(minute_count == 90)		// Maximum wait time for a wash would be 90 minutes.
			memory_wash();
		exec_commands();
	}
}
/*-----------------------------------------------------------*/

static void memory_wash(void)
{
	int x, y, z;
	uint8_t spi_chip, write_required = 0, sum, correct_val;	// write_required can be either 1, 2, or 3 to indicate which chip needs a write.
	uint32_t page, addr, byte;
	
	for(page = 0; page < 4096; page++)	// Loop through each page in memory.
	{
		addr = page << 8;
			
		for(spi_chip = 1; spi_chip < 4; spi_chip++)
		{
			x = spimem_read(spi_chip, addr, page_buff1, 256);
			y = spimem_read(spi_chip, addr, page_buff2, 256);
			z = spimem_read(spi_chip, addr, page_buff3, 256);
				
			if((x < 0) || (y < 0) || (z < 0))
			x = x;									// FAILURE_RECOVERY.
		}
			
		for(byte = 0; byte < 256; byte++)
		{
			if(page_buff1[byte] != page_buff2[byte])
			{
				if(page_buff2[byte] == page_buff3[byte])
				{
					write_required = 1;
					correct_val = page_buff2[byte];
				}
					
				if(page_buff1[byte] == page_buff3[byte])
				{
					write_required = 2;
					correct_val = page_buff3[byte];
				}
			}
			else
			{
				if(page_buff1[byte] != page_buff3[byte])
				{
					write_required = 3;
					correct_val = page_buff1[byte];
				}
			}
				
			if(write_required)
			spimem_write_h(write_required, (addr + byte), &correct_val, 1);
		}
			
	}
	minute_count = 0;
	return;
}

// This function checks the queue for a command from the obc_packet_router and the executes it.
// Otherwise it waits for a maximum of 1 minute.
static void exec_commands(void)
{
	uint8_t command, memid, status;
	uint16_t i, j;
	uint16_t packet_id, pcs;
	uint8_t* mem_ptr = 0;
	uint32_t address, length, last_len = 0, num_transfers = 0;
	uint64_t check = 0;
	clear_current_command();
	if(xQueueReceive(obc_to_mem_fifo, current_command, xTimeToWait) == pdTRUE)	// Check for a command from the OBC packet router.
	{
		command = current_command[146];
		packet_id = ((uint16_t)current_command[140]) << 8;
		packet_id += (uint16_t)current_command[139];
		pcs = ((uint16_t)current_command[138]) << 8;
		pcs += (uint16_t)current_command[137];
		memid = current_command[136];
		address =  ((uint32_t)current_command[135]) << 24;
		address += ((uint32_t)current_command[134]) << 16;
		address += ((uint32_t)current_command[133]) << 8;
		address += (uint32_t)current_command[132];
		length =  ((uint32_t)current_command[131]) << 24;
		length += ((uint32_t)current_command[130]) << 16;
		length += ((uint32_t)current_command[129]) << 8;
		length += (uint32_t)current_command[128];
		switch(command)
		{
			case	MEMORY_LOAD_ABS:
				if(!memid)
				{
					mem_ptr = address;
					for(i = 0; i < length; i++)
					{
						*(mem_ptr + i) = current_command[i];
					}
				}
				else
				{
					if(spimem_write(address, current_command, length) < 0)				// FAILURE_RECOVERY
						send_tc_execution_verify(0xFF, packet_id, pcs);
				}
				send_tc_execution_verify(1, packet_id, pcs);
			case	DUMP_REQUEST_ABS:
				clear_current_command();		// Only clears lower data section.
				last_len = 0;
				if(length > 128)
				{
					num_transfers = length / 128;
				}
				for (j = 0; j < num_transfers; j++)
				{
					if(!memid)
					{
						mem_ptr = address;
						for (i = 0; i < length; i++)
						{
							current_command[i] = *(mem_ptr + i);
						}
					}
					else
					{
						if(spimem_read(1, address, current_command, length) < 0)		// FAILURE_RECOVERY
							send_tc_execution_verify(0xFF, packet_id, pcs);
					}
					current_command[146] = MEMORY_DUMP_ABS;
					current_command[145] = num_transfers - j;
					xQueueSendToBack(mem_to_obc_fifo, current_command, (TickType_t)1);	// FAILURE_RECOVERY if this doesn't return pdTrue
					taskYIELD();	// Give the packet router a chance to downlink the dump packet.				
				}
				send_tc_execution_verify(1, packet_id, pcs);
			case	CHECK_MEM_REQUEST:
				if(!memid)
				{
					check = fletcher64(address, length);
					send_tc_execution_verify(1, packet_id, pcs);
				}
				else
				{
					check = fletcher64_on_spimem(address, length, &status);
					if(status > 1)
					{
						send_tc_execution_verify(0xFF, packet_id, pcs);
						return;
					}
					send_tc_execution_verify(1, packet_id, pcs);
				}
			default:
				return;
		}
	}
	return;
}

static void clear_current_command(void)
{
	uint8_t i;
	for(i = 0; i < 128; i++)
	{
		current_command[i] = 0;
	}
	return;
}

static void send_tc_execution_verify(uint8_t status, uint16_t packet_id, uint16_t pcs)
{
	clear_current_command();
	current_command[146] = TASK_TO_OPR_TCV;		// Request a TC verification
	current_command[145] = status;
	current_command[144] = MEMORY_TASK_ID;		// APID of this task
	current_command[140] = ((uint8_t)packet_id) >> 8;
	current_command[139] = (uint8_t)packet_id;
	current_command[138] = ((uint8_t)pcs) >> 8;
	current_command[137] = (uint8_t)pcs;
	xQueueSendToBack(mem_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY if this doesn't return pdPASS
	return;
}

static void send_event_report(uint8_t severity, uint8_t report_id, uint8_t param1, uint8_t param0)
{
	clear_current_command();
	current_command[146] = TASK_TO_OPR_EVENT;
	current_command[3] = severity;
	current_command[2] = report_id;
	current_command[1] = param1;
	current_command[0] = param0;
	xQueueSendToBack(mem_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY
	return;
}