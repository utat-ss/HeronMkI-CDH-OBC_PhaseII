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
* 11/11/2015		Added the majority of the functionality that this service requires.
*					
*					As commands are added in the future, chek_commands() should be modified to accomodate them.
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

/* Atmel library includes.			*/
#include "asf.h"
/* Common demo includes.			*/
#include "partest.h"
/*		RTC includes.				*/
#include "rtc.h"
/*		CAN functionality includes	*/
#include "can_func.h"
/*		SPI MEM includes			*/
#include "spimem.h"

/* Priorities at which the tasks are created. */
#define SCHEDULING_PRIORITY		( tskIDLE_PRIORITY + 3 )

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define SCHEDULING_PARAMETER			( 0xABCD )

/* Definitions to clarify which service subtypes represent what	*/
/* K-Service							
#define ADD_SCHEDULE					1
#define CLEAR_SCHEDULE					2
#define	SCHED_REPORT_REQUEST			3
#define SCHED_REPORT					4*/

#define MAX_COMMANDS					1023

/* Functions Prototypes. */
static void prvSchedulingTask( void *pvParameters );
void scheduling(void);
static void exec_pus_commands(void);
static int modify_schedule(uint8_t* status, uint8_t* kicked_count);
static void add_command_to_end(uint32_t new_time, uint8_t position);
static void add_command_to_beginning(uint32_t new_time, uint8_t position);
static void add_command_to_middle(uint32_t new_time, uint8_t position);
static int shift_schedule_right(uint32_t address);
static int shift_schedule_left(uint32_t address);
static void clear_schedule_buffers(void);
static void load_buff1_to_buff0(void);
static int check_schedule(void);
static int clear_schedule(void);
static void clear_temp_array(void);
static void clear_current_command(void);
static int report_schedule(void);
static void send_tc_execution_verify(uint8_t status, uint16_t packet_id, uint16_t pcs);
static void send_event_report(uint8_t severity, uint8_t report_id, uint8_t param1, uint8_t param0);


/* Local variables for scheduling */
static uint32_t num_commands, next_command_time, furthest_command_time;
static uint8_t temp_arr[256];
static uint8_t current_command[DATA_LENGTH + 10];
static uint8_t sched_buff0[256], sched_buff1[256];
static int x;

/************************************************************************/
/* SCHEDULING (Function)												*/
/* @Purpose: This function is used to create the scheduling task.		*/
/************************************************************************/
void scheduling( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvSchedulingTask,					/* The function that implements the task. */
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
	x = -1;
	x = spimem_read(SCHEDULE_BASE, temp_arr, 4);			// FAILURE_RECOVERY required here.
	num_commands = ((uint32_t)temp_arr[3]) << 24;
	num_commands = ((uint32_t)temp_arr[2]) << 16;
	num_commands = ((uint32_t)temp_arr[1]) << 8;
	num_commands = (uint32_t)temp_arr[0];
	x = spimem_read(SCHEDULE_BASE+4, temp_arr, 4);
	next_command_time = ((uint32_t)temp_arr[3]) << 24;
	next_command_time = ((uint32_t)temp_arr[2]) << 16;
	next_command_time = ((uint32_t)temp_arr[1]) << 8;
	next_command_time = (uint32_t)temp_arr[0];
	x = spimem_read(SCHEDULE_BASE + 4 + ((num_commands - 1) * 8), temp_arr, 4);
	furthest_command_time = ((uint32_t)temp_arr[3]) << 24;
	furthest_command_time = ((uint32_t)temp_arr[2]) << 16;
	furthest_command_time = ((uint32_t)temp_arr[1]) << 8;
	furthest_command_time = (uint32_t)temp_arr[0];
	clear_schedule_buffers();
	clear_temp_array();
	clear_current_command();
	
	/* @non-terminating@ */	
	for( ;; )
	{
		exec_pus_commands();
		check_schedule();
	}
}
/*-----------------------------------------------------------*/

// static helper functions may be defined below.

static void exec_pus_commands(void)
{
	uint16_t packet_id, pcs;
	uint8_t status, kicked_count;
	if(xQueueReceive(obc_to_sched_fifo, current_command, (TickType_t)1000) == pdTRUE)	// Only block for a single second.
	{
		packet_id = ((uint16_t)current_command[140]) << 8;
		packet_id += (uint16_t)current_command[139];
		pcs = ((uint16_t)current_command[138]) << 8;
		pcs += (uint16_t)current_command[137];
		switch(current_command[146])
		{
			case ADD_SCHEDULE:
				x = modify_schedule(&status, &kicked_count);
				if(status == -1)
					send_tc_execution_verify(0xFF, packet_id, pcs);			// The Schedule modification failed
				if(status == 2)
					send_event_report(1, KICK_COM_FROM_SCHEDULE, kicked_count, 0);		// the modification kicked out commands.
				if(status == 1)
					send_tc_execution_verify(1, packet_id, pcs);			// modification succeeded without a hitch
				check_schedule();
			case CLEAR_SCHEDULE:
				if(clear_schedule() < 0)
					send_tc_execution_verify(0xFF, packet_id, pcs);
				else
					send_tc_execution_verify(1, packet_id, pcs);
			case SCHED_REPORT_REQUEST:
				if(report_schedule() < 0)
					send_tc_execution_verify(0xFF, packet_id, pcs);
				else
					send_tc_execution_verify(1, packet_id, pcs);			
			default:
				return;
		}
	}
	return;
}

static int modify_schedule(uint8_t* status, uint8_t* kicked_count)
{
	uint8_t num_new_commands = current_command[136];
	uint8_t i;
	uint32_t new_time;
	*kicked_count = 0;
		
	for(i = 0; i < num_new_commands; i++)			// out of the schedule.
	{
		new_time = ((uint32_t)current_command[135 - (i * 8)]) << 24;
		new_time += ((uint32_t)current_command[134 - (i * 8)]) << 16;
		new_time += ((uint32_t)current_command[133 - (i * 8)]) << 8;
		new_time += (uint32_t)current_command[132 - (i * 8)];
		
		if((num_commands == MAX_COMMANDS) && (new_time >= furthest_command_time))
		{
			*status = -1;		// Indicates failure
			return i;			// Number of command which was successfully placed in the schedule.
		}
		if((num_commands == MAX_COMMANDS) && (new_time <= furthest_command_time))
		{
			*status = 2;		// Indicates a command was kicked out of the schedule, but no failure.
			(*kicked_count)++;
		}
		if(new_time >= furthest_command_time)
			add_command_to_end(new_time, 135 - (i * 8));
		if(new_time < next_command_time)
			add_command_to_beginning(new_time, 135 - (i * 8));
		else
			add_command_to_middle(new_time, 135 - (i * 8));
		if(num_commands < MAX_COMMANDS)
			num_commands++;
	}
	temp_arr[0] = (uint8_t)num_commands;
	temp_arr[1] = (uint8_t)(num_commands >> 8);
	temp_arr[2] = (uint8_t)(num_commands >> 16);
	temp_arr[3] = (uint8_t)(num_commands >> 24);
	x = spimem_write(SCHEDULE_BASE, temp_arr, 4);				// update the num_commands within spi memory.
	*status = 1;
	return 1;
}

// position is meant to be the position in the current_command[] array.
static void add_command_to_end(uint32_t new_time, uint8_t position)
{
	if(num_commands == MAX_COMMANDS)
		return;																						// USAGE ERROR
	x = spimem_write((SCHEDULE_BASE + 4 + (num_commands * 8)), current_command + position, 8);		// FAILURE_RECOVERY
	furthest_command_time = new_time;
	return;
}

static void add_command_to_beginning(uint32_t new_time, uint8_t position)
{
	next_command_time = new_time;
	x = shift_schedule_right(SCHEDULE_BASE + 4);														// FAILURE_RECOVERY
	x = spimem_write(SCHEDULE_BASE + 4, current_command + position, 8);
	return;
}

static void add_command_to_middle(uint32_t new_time, uint8_t position)
{
	uint16_t i;
	uint32_t stored_time = 0;
	uint8_t time_arr[4];
	for(i = 0; i < num_commands; i++)
	{
		if(spimem_read((SCHEDULE_BASE + 4 + num_commands * 8), time_arr, 4) < 0)						// FAILURE_RECOVERY
			return;
		stored_time = ((uint32_t)time_arr[0]) << 24;
		stored_time += ((uint32_t)time_arr[1]) << 16;
		stored_time += ((uint32_t)time_arr[2]) << 8;
		stored_time += (uint32_t)time_arr[3];
		if(new_time >= stored_time)
		{
			shift_schedule_right(SCHEDULE_BASE + 4 + (i + 1) * 8);
			x = spimem_write(SCHEDULE_BASE + 4 + (i + 1) * 8, current_command + position, 8);
			return;
		}
	}
	return;
}

// Shifts the entire schedule by 8 B starting @ address. (The furthest command in the schedule gets droppped if necessary)
static int shift_schedule_right(uint32_t address)
{
	uint8_t num_pages, i;
	num_pages = ((num_commands * 8) - (address - (SCHEDULE_BASE + 4))) / 256;
	if(((num_commands * 8) - (address - (SCHEDULE_BASE + 4))) % 256)
		num_pages++;

	if(spimem_read(SCHEDULE_BASE + 8096, temp_arr, 256) < 0)				// Store the page of memory which may be overwritten.
		return -1;
	if(spimem_read(address, sched_buff0, 256) < 0)
		return -1;
	if(spimem_read(address + 256, sched_buff1, 256) < 0)
		return -1;
	for(i = 0; i < num_pages; i++)
	{
		if(spimem_write((address + (i * 256) + 8), sched_buff0, 256) < 0)
			return -1;
		load_buff1_to_buff0();
		if(spimem_read((address + (i + 1) * 256), sched_buff1, 256) < 0)
			return -1;
	}

	if(spimem_write(SCHEDULE_BASE + 8096, temp_arr, 256) < 0)				// Restore the page which may have been overwritten.
		return -1;
	return 1;
}

static int shift_schedule_left(uint32_t address)
{
	uint8_t num_pages, i;
	num_pages = ((num_commands * 8) - (address - (SCHEDULE_BASE + 4))) / 256;
	if(((num_commands * 8) - (address - (SCHEDULE_BASE + 4))) % 256)
		num_pages++;

	for(i = 0; i < num_pages; i++)
	{
		if(spimem_read((address + (i * 256) - 8), sched_buff0, 256) < 0)
			return -1;
		if(spimem_write((address + (i * 256) - 8), sched_buff0, 256) < 0)
			return -1;
	}
	return 1;
}

static void clear_schedule_buffers(void)
{
	uint16_t i;
	for(i = 0; i < 256; i++)
	{
		sched_buff0[i] = 0;
		sched_buff1[i] = 0;
	}
	return;
}

static void load_buff1_to_buff0(void)
{
	uint16_t i;
	for(i = 0; i < 256; i++)
	{
		sched_buff0[i] = sched_buff1[i];
	}
	return;
}

static int check_schedule(void)
{
	if(next_command_time >= CURRENT_TIME)
	{
		// Execute the Command.
		shift_schedule_left(SCHEDULE_BASE + 12);				// Shift out the command which was just executed.
		num_commands--;
		temp_arr[0] = (uint8_t)num_commands;
		temp_arr[1] = (uint8_t)(num_commands >> 8);
		temp_arr[2] = (uint8_t)(num_commands >> 16);
		temp_arr[3] = (uint8_t)(num_commands >> 24);
		x = spimem_write(SCHEDULE_BASE, temp_arr, 4);				// update the num_commands within SPI memory.
		if(x < 0)
			return -1;												// FAILURE_RECOVERY
	}
	return 1;
}

static int clear_schedule(void)
{
	uint8_t i;
	clear_temp_array();
	for(i = 0; i < 32; i++)
	{
		x = spimem_write(SCHEDULE_BASE + i * 256, temp_arr, 256);			// FAILURE_RECOVERY
		if(x < 0)
			return -1;
	}
	num_commands = 0;
	return 1;
}

static void clear_temp_array(void)
{
	uint16_t i;
	for(i = 0; i < 256; i++)
	{
		temp_arr[i] = 0;
	}
	return;
}

static void clear_current_command(void)
{
	uint8_t i;
	for(i = 0; i < (DATA_LENGTH + 10); i++)
	{
		current_command[i] = 0;
	}
	return;
}

static int report_schedule(void)
{
	uint8_t i, num_pages;
	num_pages = (4 + num_commands * 8) /256;
	if((4 + num_commands * 8) % 256)
		num_pages++;
	clear_current_command();
	current_command[146] = SCHED_REPORT;
	for(i = 0; i < (num_pages * 2); i++)
	{
		current_command[145] = (num_pages * 2) - i;
		current_command[144] = i;
		spimem_read(SCHEDULE_BASE + i * 128, temp_arr, 128);
		if(xQueueSendToBack(sched_to_obc_fifo, temp_arr, (TickType_t)10) != pdPASS)
			return -1;						// FAILURE_RECOVERY
	}
	return 1;
}

// status = 0x01 for success, 0xFF for failure.
static void send_tc_execution_verify(uint8_t status, uint16_t packet_id, uint16_t pcs)
{
	clear_current_command();
	current_command[146] = TASK_TO_OPR_TCV;		// Request a TC verification
	current_command[145] = status;
	current_command[144] = SCHEDULING_TASK_ID;			// APID of this task
	current_command[140] = ((uint8_t)packet_id) >> 8;
	current_command[139] = (uint8_t)packet_id;
	current_command[138] = ((uint8_t)pcs) >> 8;
	current_command[137] = (uint8_t)pcs;
	xQueueSendToBack(sched_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY if this doesn't return pdPASS
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
	xQueueSendToBack(sched_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY
	return;
}