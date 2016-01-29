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
	*	11/12/2015		Adding in functionality for TC execution verification, and event reporting to ground.
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

#include "can_func.h"

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
void memory_manage_kill(uint8_t killer);
static void memory_wash(void);
static void exec_commands(void);
static void exec_commands_H(void);
static void clear_current_command(void);
static void send_tc_execution_verify(uint8_t status, uint16_t packet_id, uint16_t psc);
static void send_event_report(uint8_t severity, uint8_t report_id, uint8_t param1, uint8_t param0);

/* Local variables for memory management */
static uint8_t minute_count;
static uint8_t current_command[DATA_LENGTH + 10];
static const TickType_t xTimeToWait = 60000;	// Number entered here corresponds to the number of ticks we should wait (1 minute)

/************************************************************************/
/* MEMORY_WASH (Function)												*/
/* @Purpose: This function is used to create the memory washing task.	*/
/************************************************************************/
TaskHandle_t memory_manage( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		TaskHandle_t temp_HANDLE = 0;
		xTaskCreate( prvMemoryManageTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) MM_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					MEMORY_MANAGE_PRIORITY, 			/* The priority assigned to the task. */
					&temp_HANDLE );								/* The task handle is not required, so NULL is passed. */
					
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	return temp_HANDLE;
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
	SPI_HEALTH1 = 1;
	SPI_HEALTH2 = 1;
	SPI_HEALTH3 = 1;
	memory_wash();					// This will cause the SPI health variables to be updated immediately.
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
/* static helper functions below */

/************************************************************************/
/* MEMORY_WASH															*/
/* @Purpose: Given that all 3 SPIMEM chips are working properly, this	*/
/* function reads through each page in memory on all 3 spi chips and	*/
/* compares the results of what is read. It then uses a voting algorithm*/
/* to rewrite areas of memory which may have been subjected to a bitflip*/
/* If an anomaly is detected and cannot be rectified, an error report	*/
/* is sent to the FDIR task.											*/
/************************************************************************/
static void memory_wash(void)
{
	int x, y, z, check, attempts;
	uint8_t spi_chip, write_required = 0, correct_val;	// write_required can be either 1, 2, or 3 to indicate which chip needs a write.
	uint8_t check_val = 0;
	uint32_t page, addr, byte;
	
	if(INTERNAL_MEMORY_FALLBACK)	// No washing while in internal memory fallback mode.
		return;
	
	//adding this to check for MEM_SPIMEM_CHIPS_ERROR
	if(!SPI_HEALTH1 && !SPI_HEALTH2 && !SPI_HEALTH3){
		errorASSERT(MEMORY_TASK_ID, 0, MEM_SPIMEM_CHIPS_ERROR, 0, 0);
		//3 dead spimem chips is highsev?
		//no mutex
		return;
	}
	
	
	if(!SPI_HEALTH1 || !SPI_HEALTH2 || !SPI_HEALTH3)
	{
		// If one of the chips is dead, we can't do any memory washing.
		return;
	}
	
	for(page = 0; page < 4096; page++)	// Loop through each page in memory.
	{
		addr = page << 8;
		
		
		//TODO: make this less ugly
		for(spi_chip = 1; spi_chip < 4; spi_chip++)
		{
			x = spimem_read_alt(spi_chip, addr, page_buff1, 256);
			y = spimem_read_alt(spi_chip, addr, page_buff2, 256);
			z = spimem_read_alt(spi_chip, addr, page_buff3, 256);
				
			if((x < 0) || (y < 0) || (z < 0)){
				x = spimem_read_alt(spi_chip, addr, page_buff1, 256);
				y = spimem_read_alt(spi_chip, addr, page_buff2, 256);
				z = spimem_read_alt(spi_chip, addr, page_buff3, 256);
				
				if((x < 0) || (y < 0) || (z < 0)){
					x = spimem_read_alt(spi_chip, addr, page_buff1, 256);
					y = spimem_read_alt(spi_chip, addr, page_buff2, 256);
					z = spimem_read_alt(spi_chip, addr, page_buff3, 256);
					if((x < 0) || (y < 0) || (z < 0)){errorREPORT(MEMORY_TASK_ID, 0, MEM_SPIMEM_MEM_WASH_ERROR, 0);}
					}
			}
												
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
			{
				attempts = 0; check = -1;
				while (attempts<3 && check<0){
					check = spimem_write_h(write_required,(addr+byte), &correct_val, 1);
					attempts++;
				}
				if (check<0){errorREPORT(MEMORY_TASK_ID, 0, MEM_SPIMEM_MEM_WASH_ERROR, 0);}
				send_event_report(1, BIT_FLIP_DETECTED, 0, 0);		
			}
			
			attempts = 0; check = -1;
			
			while (attempts<3 && check<0){
				check = spimem_read_alt(write_required, (addr+byte), &check_val, 1);
				attempts++;
			}
			if (check<0){errorREPORT(MEMORY_TASK_ID, 0, MEM_SPIMEM_MEM_WASH_ERROR, 0);}
		
			if(check_val != correct_val)
			{
				// SPI_CHIP: write_required has something wrong with it
				if(write_required == 1)
					SPI_HEALTH1 = 0;
				if(write_required == 2)
					SPI_HEALTH2 = 0;
				if(write_required == 3)
					SPI_HEALTH3 = 0;
				// Send an error report to the FDIR task. (The FDIR task will send the event report)
				return;
			}
		}
		send_event_report(1, MEMORY_WASH_FINISHED, 0, 0);
	}
	minute_count = 0;
	return;
}

/************************************************************************/
/* EXEC_PUS_COMMANDS													*/
/* @Purpose: Attempts to receive from obc_to_mem_fifo, executes			*/
/* different commands depending on what was received. Otherwise, it		*/
/* waits for a maximum of 1 minute.										*/
/************************************************************************/
static void exec_commands(void)
{
	clear_current_command();
	if(xQueueReceiveTask(MEMORY_TASK_ID, 0, obc_to_mem_fifo, current_command, xTimeToWait) == pdTRUE)	// Check for a command from the OBC packet router.
		exec_commands_H();
	else if(xQueueReceive(sched_to_memory_fifo, current_command, (TickType_t)1))
		exec_commands_H();
	return;
}

static void exec_commands_H(void)
{
	uint8_t command, memid, status;
	uint16_t i, j;
	uint16_t packet_id, psc;
	uint8_t* mem_ptr = 0;
	uint32_t address, length, num_transfers = 0;
	uint32_t* temp_address = 0;
	uint64_t check = 0; 
	int attempts = 0;
	command = current_command[146];
	packet_id = ((uint16_t)current_command[140]) << 8;
	packet_id += (uint16_t)current_command[139];
	psc = ((uint16_t)current_command[138]) << 8;
	psc += (uint16_t)current_command[137];
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
				attempts = 0; check = -1;
					
				while (attempts<3 && check<0){
					check = spimem_write(address, current_command, length);
					attempts++;
				}
					
				if (check <0){
					errorREPORT(MEMORY_TASK_ID, 0, MEM_OTHER_SPIMEM_ERROR, NULL); //didn't have enough parameters - just putting NULL for now
					send_tc_execution_verify(0xFF, packet_id, psc);}
			}
			send_tc_execution_verify(1, packet_id, psc);
		case	DUMP_REQUEST_ABS:
			clear_current_command();		// Only clears lower data section.
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
					check = -1; attempts = 0;
					while (attempts<3 && check<0){
						check = spimem_read(address, current_command, length);
						attempts++;
					}
						
					if (check<0){
						errorREPORT(MEMORY_TASK_ID, 0, MEM_OTHER_SPIMEM_ERROR,NULL); //didn't have enough parameters - just putting NULL for now
						send_tc_execution_verify(0xFF, packet_id, psc);
					}
						
				}
				current_command[146] = MEMORY_DUMP_ABS;
				current_command[145] = num_transfers - j;
				xQueueSendToBackTask(MEMORY_TASK_ID, 1, mem_to_obc_fifo, current_command, (TickType_t)1);	// FAILURE_RECOVERY if this doesn't return pdTrue
				taskYIELD();	// Give the packet router a chance to downlink the dump packet.				
			}
			send_tc_execution_verify(1, packet_id, psc);
		case	CHECK_MEM_REQUEST:
			if(!memid)
			{
				temp_address = address;
				check = fletcher64(temp_address, length);
				send_tc_execution_verify(1, packet_id, psc);
			}
			else
			{
				check = fletcher64_on_spimem(address, length, &status);
				if(status > 1)
				{
					send_tc_execution_verify(0xFF, packet_id, psc);
					return;
				}
				send_tc_execution_verify(1, packet_id, psc);
			}
			current_command[146] = MEMORY_CHECK_ABS;
			current_command[7] = (uint8_t)((check & 0xFF00000000000000) >> 56);
			current_command[6] = (uint8_t)((check & 0x00FF000000000000) >> 48);
			current_command[5] = (uint8_t)((check & 0x0000FF0000000000) >> 40);
			current_command[4] = (uint8_t)((check & 0xFF0000FF00000000) >> 32);
			current_command[3] = (uint8_t)((check & 0xFF000000FF000000) >> 24);
			current_command[2] = (uint8_t)((check & 0xFF00000000FF0000) >> 26);
			current_command[1] = (uint8_t)((check & 0xFF0000000000FF00) >> 8);
			current_command[0] = (uint8_t)(check & 0x00000000000000FF);
			xQueueSendToBackTask(MEMORY_TASK_ID, 1, mem_to_obc_fifo, current_command, (TickType_t)1);
		default:
			return;
	}
	return;
}

/************************************************************************/
/* CLEAR_CURRENT_COMMAND												*/
/* @Purpose: clears the array current_command[]							*/
/************************************************************************/
static void clear_current_command(void)
{
	uint8_t i;
	for(i = 0; i < (DATA_LENGTH + 10); i++)
	{
		current_command[i] = 0;
	}
	return;
}

/************************************************************************/
/* SEND_TC_EXECUTION_VERIFY												*/
/* @Purpose: sends an execution verification to the OBC_PACKET_ROUTER	*/
/* which then attempts to downlink the telemetry to ground.				*/
/* @param: status: 0x01 = Success, 0xFF = failure.						*/
/* @param: packet_id: The first 2B of the PUS packet.					*/
/* @param: psc: the next 2B of the PUS packet.							*/
/************************************************************************/
static void send_tc_execution_verify(uint8_t status, uint16_t packet_id, uint16_t psc)
{
	clear_current_command();
	current_command[146] = TASK_TO_OPR_TCV;		// Request a TC verification
	current_command[145] = status;
	current_command[144] = MEMORY_TASK_ID;		// APID of this task
	current_command[140] = ((uint8_t)packet_id) >> 8;
	current_command[139] = (uint8_t)packet_id;
	current_command[138] = ((uint8_t)psc) >> 8;
	current_command[137] = (uint8_t)psc;
	xQueueSendToBackTask(MEMORY_TASK_ID, 1, mem_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY if this doesn't return pdPASS
	return;
}

/************************************************************************/
/* SEND_EVENT_REPORT		                                            */
/* @Purpose: sends a message to the OBC_PACKET_ROUTER using				*/
/* mem_to_obc_fifo. The intent is to an event report downlinked to	*/
/* the ground station.													*/
/* @param: severity: 1 = Normal.										*/
/* @param: report_id: Unique to the event report, ex: BIT_FLIP_DETECTED */
/* @param: param1,0 extra information that can be sent to ground.		*/
/************************************************************************/
static void send_event_report(uint8_t severity, uint8_t report_id, uint8_t param1, uint8_t param0)
{
	clear_current_command();
	current_command[146] = TASK_TO_OPR_EVENT;
	current_command[145] = severity;
	current_command[136] = report_id;
	current_command[135] = 2;
	current_command[134] = 0x00;
	current_command[133] = 0x00;
	current_command[132] = 0x00;
	current_command[131] = param0;
	current_command[130] = 0x00;
	current_command[129] = 0x00;
	current_command[128] = 0x00;
	current_command[127] = param1;
	xQueueSendToBackTask(MEMORY_TASK_ID, 1, mem_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY
	return;
}

// This function will kill this task.
// If it is being called by this task 0 is passed, otherwise it is probably the FDIR task and 1 should be passed.
void memory_manage_kill(uint8_t killer)
{
	// Free the memory that this task allocated.
	//vPortFree(current_command);
	//vPortFree(minute_count);
	//vPortFree(xTimeToWait);
	// Kill the task.
	if(killer)
		vTaskDelete(memory_manage_HANDLE);
	else
		vTaskDelete(NULL);
	return;
}