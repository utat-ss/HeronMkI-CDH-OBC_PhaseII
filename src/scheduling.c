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
* 11/25/2015		It occurred to me that it's going to make life so much better in the long run if each of the commands
*					put in the schedule has it's own unique ID that we can keep track of as well as a "status" byte.
*					Commands in the OBSW schedule may have a status, but this status shall be ignored except for when
*					it is time to send a report to ground on the status of the scheduled command.
*
*					The use of cIDs simplifies the implementation of reports on failed / succeeded scheduled commands.
*
*					Note that this causes many changes to occur in GPR code and scheduling code as commands are now 16B long.
*					(I am now effecting these changes)
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

#define MAX_COMMANDS					511

/* Functions Prototypes. */
static void prvSchedulingTask( void *pvParameters );
TaskHandle_t scheduling(void);
void scheduling_kill(uint8_t killer);
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
static void send_tc_execution_verify(uint8_t status, uint16_t packet_id, uint16_t psc);
static void send_event_report(uint8_t severity, uint8_t report_id, uint8_t param1, uint8_t param0);
static int generate_command_report(uint16_t cID, uint8_t status);


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
TaskHandle_t scheduling( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		TaskHandle_t temp_HANDLE = 0;
		xTaskCreate( prvSchedulingTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) SCHEDULING_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					SCHEDULING_PRIORITY, 			/* The priority assigned to the task. */
					&temp_HANDLE );								/* The task handle is not required, so NULL is passed. */
	return temp_HANDLE;
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
	x = spimem_read(SCHEDULE_BASE + 4 + ((num_commands - 1) * 16), temp_arr, 4);
	furthest_command_time = ((uint32_t)temp_arr[3]) << 24;
	furthest_command_time = ((uint32_t)temp_arr[2]) << 16;
	furthest_command_time = ((uint32_t)temp_arr[1]) << 8;
	furthest_command_time = (uint32_t)temp_arr[0];
	scheduling_on = 1;
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
/* static helper functions below */

/************************************************************************/
/* EXEC_PUS_COMMANDS													*/
/* @Purpose: Attempts to receive from obc_to_sched_fifo, executes		*/
/* different commands depending on what was received.					*/
/************************************************************************/
static void exec_pus_commands(void)
{
	uint16_t packet_id, psc;
	uint8_t status, kicked_count;
	if(xQueueReceive(obc_to_sched_fifo, current_command, (TickType_t)1000) == pdTRUE)	// Only block for a single second.
	{
		packet_id = ((uint16_t)current_command[140]) << 8;
		packet_id += (uint16_t)current_command[139];
		psc = ((uint16_t)current_command[138]) << 8;
		psc += (uint16_t)current_command[137];
		switch(current_command[146])
		{
			case ADD_SCHEDULE:
				x = modify_schedule(&status, &kicked_count);
				if(status == -1)
					send_tc_execution_verify(0xFF, packet_id, psc);			// The Schedule modification failed
				if(status == 2)
					send_event_report(1, KICK_COM_FROM_SCHEDULE, kicked_count, 0);		// the modification kicked out commands.
				if(status == 1)
					send_tc_execution_verify(1, packet_id, psc);			// modification succeeded without a hitch
				check_schedule();
			case CLEAR_SCHEDULE:
				if(clear_schedule() < 0)
					send_tc_execution_verify(0xFF, packet_id, psc);
				else
					send_tc_execution_verify(1, packet_id, psc);
			case SCHED_REPORT_REQUEST:
				if(report_schedule() < 0)
					send_tc_execution_verify(0xFF, packet_id, psc);
				else
					send_tc_execution_verify(1, packet_id, psc);
			case PAUSE_SCHEDULE:
				scheduling_on = 0;
			case RESUME_SCHEDULE:
				scheduling_on = 1;
			default:
				return;
		}
	}
	return;
}

/************************************************************************/
/* MODIFY_SCHEDULE														*/
/* @Purpose: When an ADD_SCHEDULE command comes in, we want to add		*/
/* commands as necessary to SPIMEM.										*/
/* @param: *status: 1 = success, -1 = failure, 2 = command kicked out.	*/
/* @param: *kicked_count: if *status == 2, is set to the number of		*/
/* commands which were kicked out of the schedule.						*/
/* @return: -1 = something went wrong, 1 = action succeeded.			*/
/************************************************************************/
static int modify_schedule(uint8_t* status, uint8_t* kicked_count)
{
	uint8_t num_new_commands = current_command[136];
	uint8_t i;
	uint32_t new_time;
	*kicked_count = 0;
		
	for(i = 0; i < num_new_commands; i++)			// out of the schedule.
	{
		new_time = ((uint32_t)current_command[135 - (i * 16)]) << 24;
		new_time += ((uint32_t)current_command[134 - (i * 16)]) << 16;
		new_time += ((uint32_t)current_command[133 - (i * 16)]) << 8;
		new_time += (uint32_t)current_command[132 - (i * 16)];
		
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
			add_command_to_end(new_time, 135 - (i * 16));
		if(new_time < next_command_time)
			add_command_to_beginning(new_time, 135 - (i * 16));
		else
			add_command_to_middle(new_time, 135 - (i * 16));
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

/************************************************************************/
/* ADD_COMMAND_TO_...													*/
/* @Purpose: places a new command in the schedule either at the begin	*/
/* -ing, middle or end.													*/
/* @param: new_time: the time that the new command is to be executed.	*/
/* @param: position: where the command is located in current_command[]	*/
/* @Note: The furthest command in the schedule gets dropped if necessary*/
/************************************************************************/
static void add_command_to_end(uint32_t new_time, uint8_t position)
{
	if(num_commands == MAX_COMMANDS)
		return;																							// USAGE ERROR
	x = spimem_write((SCHEDULE_BASE + 4 + (num_commands * 16)), current_command + position, 16);		// FAILURE_RECOVERY
	furthest_command_time = new_time;
	return;
}

static void add_command_to_beginning(uint32_t new_time, uint8_t position)
{
	next_command_time = new_time;
	x = shift_schedule_right(SCHEDULE_BASE + 4);														// FAILURE_RECOVERY
	x = spimem_write(SCHEDULE_BASE + 4, current_command + position, 16);
	return;
}

static void add_command_to_middle(uint32_t new_time, uint8_t position)
{
	uint16_t i;
	uint32_t stored_time = 0;
	uint8_t time_arr[4];
	for(i = 0; i < num_commands; i++)
	{
		if(spimem_read((SCHEDULE_BASE + 4 + num_commands * 16), time_arr, 4) < 0)						// FAILURE_RECOVERY
			return;
		stored_time = ((uint32_t)time_arr[0]) << 24;
		stored_time += ((uint32_t)time_arr[1]) << 16;
		stored_time += ((uint32_t)time_arr[2]) << 8;
		stored_time += (uint32_t)time_arr[3];
		if(new_time >= stored_time)
		{
			shift_schedule_right(SCHEDULE_BASE + 4 + (i + 1) * 16);
			x = spimem_write(SCHEDULE_BASE + 4 + (i + 1) * 16, current_command + position, 16);
			return;
		}
	}
	return;
}

/************************************************************************/
/* SHIFT_SCHEDULE_RIGHT													*/
/* @Purpose: shifts the schedule stored in spimem right by 8 bytes		*/
/* starting at "address"												*/
/* @param: address: the location in memory we want to shift right from	*/
/* @Note: The furthest command in the schedule gets dropped if necessary*/
/* @return: -1 = something went wrong, 1 = action succeeded.			*/
/************************************************************************/
static int shift_schedule_right(uint32_t address)
{
	uint8_t num_pages, i;
	num_pages = ((num_commands * 16) - (address - (SCHEDULE_BASE + 4))) / 256;
	if(((num_commands * 16) - (address - (SCHEDULE_BASE + 4))) % 256)
		num_pages++;

	if(spimem_read(SCHEDULE_BASE + 8192, temp_arr, 256) < 0)				// Store the page of memory which may be overwritten.
		return -1;
	if(spimem_read(address, sched_buff0, 256) < 0)
		return -1;
	if(spimem_read(address + 256, sched_buff1, 256) < 0)
		return -1;
	for(i = 0; i < num_pages; i++)
	{
		if(spimem_write((address + (i * 256) + 16), sched_buff0, 256) < 0)
			return -1;
		load_buff1_to_buff0();
		if(spimem_read((address + (i + 1) * 256), sched_buff1, 256) < 0)
			return -1;
	}

	if(spimem_write(SCHEDULE_BASE + 8192, temp_arr, 256) < 0)				// Restore the page which may have been overwritten.
		return -1;
	return 1;
}

/************************************************************************/
/* SHIFT_SCHEDULE_LEFT													*/
/* @Purpose: shifts the schedule stored in spimem left by 8 bytes		*/
/* starting at "address"												*/
/* @param: address: the location in memory we want to shift left from	*/
/* @return: -1 = something went wrong, 1 = action succeeded.			*/
/* @Note: Remember to update next_command_time if it's going to change	*/
/************************************************************************/
static int shift_schedule_left(uint32_t address)
{
	uint8_t num_pages, i;
	num_pages = ((num_commands * 16) - (address - (SCHEDULE_BASE + 4))) / 256;
	if(((num_commands * 16) - (address - (SCHEDULE_BASE + 4))) % 256)
		num_pages++;

	for(i = 0; i < num_pages; i++)
	{
		if(spimem_read((address + i * 256), sched_buff0, 256) < 0)
			return -1;
		if(spimem_write((address + (i * 256) - 16), sched_buff0, 256) < 0)
			return -1;
	}
	return 1;
}

/************************************************************************/
/* CLEAR_SCHEDULE_BUFFERS												*/
/* @Purpose: clears sched_buff0[] and sched_buff1[]						*/
/************************************************************************/
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

/************************************************************************/
/* LOAD_BUFF1_TO_BUFF0													*/
/* @Purpose: copies the contents of sched_buff1 into sched_buff0		*/
/************************************************************************/
static void load_buff1_to_buff0(void)
{
	uint16_t i;
	for(i = 0; i < 256; i++)
	{
		sched_buff0[i] = sched_buff1[i];
	}
	return;
}

/************************************************************************/
/* CHECK_SCHEDULE														*/
/* @Purpose: if next_command_time >= CURRENT_TIME, then a command needs	*/
/* to be executed or forwarded to the correct task / SSM.				*/
/* This function will then shift command out of the schedule.			*/
/* @return: -1 = something went wrong, 1 = action succeeded.			*/
/*		-2 = scheduling is currently paused.							*/
/************************************************************************/
static int check_schedule(void)
{
	uint8_t status = 0x01;										// This variable is going to contain the status returned
	uint16_t cID, i;
	uint8_t command_array[16];
	if(!scheduling_on)
	{
		return -2;		// Scheduling is currently paused.
	}
	for (i = 0; i < 16; i++)
	{
		command_array[i] = 0;
	}
	if(next_command_time <= CURRENT_TIME)						// from whatever command needs to be executed below, assume it is used for now.
	{
		spimem_read(SCHEDULE_BASE + 4, command_array, 16);
		cID = ((uint16_t)command_array[7]) << 8;
		cID += (uint16_t)command_array[8];
		// status = exec_k_command();
		if(status == 0xFF)										// The scheduled command failed.
		{
			// Retry for a maximum of 3 tries.
			// Still failing: Send a failure message to the FDIR Process and wait for signal from FDIR. FAILURE_RECOVERY
			// Still failing: (What FDIR should do: ) Send a message to the ground scheduling service letting it know that the command failed.
		}
		else
		{
			status = 1;
			generate_command_report(cID, status);				// Send a command completion report to the groundstation.
		}
			
		shift_schedule_left(SCHEDULE_BASE + 20);				// Shift out the command which was just executed.
		num_commands--;
		temp_arr[0] = (uint8_t)num_commands;
		temp_arr[1] = (uint8_t)(num_commands >> 8);
		temp_arr[2] = (uint8_t)(num_commands >> 16);
		temp_arr[3] = (uint8_t)(num_commands >> 24);
		x = spimem_write(SCHEDULE_BASE, temp_arr, 4);				// update the num_commands within SPI memory.
		if(x < 0)
			return -1;												// FAILURE_RECOVERY
		x = spimem_read(SCHEDULE_BASE+4, temp_arr, 4);
		next_command_time = ((uint32_t)temp_arr[3]) << 24;			// update the next_command_time
		next_command_time = ((uint32_t)temp_arr[2]) << 16;
		next_command_time = ((uint32_t)temp_arr[1]) << 8;
		next_command_time = (uint32_t)temp_arr[0];
	}
	return 1;
}

static int generate_command_report(uint16_t cID, uint8_t status)
{
	clear_current_command();
	current_command[146] = COMPLETED_SCHED_COM_REPORT;
	current_command[2] = (uint8_t)((cID & 0xFF00) >> 8);
	current_command[1] = (uint8_t)(cID & 0x00FF);
	current_command[0] = status;
	xQueueSendToBack(sched_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY if this doesn't return pdPASS
	return;
}

/************************************************************************/
/* CLEAR_SCHEDULE														*/
/* @Purpose: Writes 0 to each page in the section of memory allocated	*/
/* to the schedule.														*/
/* @return: function succeeded = 1, -1 = something went wrong.			*/
/************************************************************************/
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

/************************************************************************/
/* CLEAR_TEMP_ARRAY														*/
/* @Purpose: clears the array temp_array[]								*/
/************************************************************************/
static void clear_temp_array(void)
{
	uint16_t i;
	for(i = 0; i < 256; i++)
	{
		temp_arr[i] = 0;
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
/* REPORT_SCHEDULE														*/
/* @Purpose: This function will downlink the entire schedule to ground	*/
/* by sending 128B chunks to the OBC_PACKET_ROUTER, which in turn will	*/
/* attempt to downlink the telemetry to ground.							*/
/************************************************************************/
static int report_schedule(void)
{
	uint8_t i, num_pages;
	num_pages = (4 + num_commands * 16) /256;
	if((4 + num_commands * 16) % 256)
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
	current_command[144] = SCHEDULING_TASK_ID;			// APID of this task
	current_command[140] = ((uint8_t)packet_id) >> 8;
	current_command[139] = (uint8_t)packet_id;
	current_command[138] = ((uint8_t)psc) >> 8;
	current_command[137] = (uint8_t)psc;
	xQueueSendToBack(sched_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY if this doesn't return pdPASS
	return;
}

/************************************************************************/
/* SEND_EVENT_REPORT		                                            */
/* @Purpose: sends a message to the OBC_PACKET_ROUTER using				*/
/* sched_to_obc_fifo. The intent is to an event report downlinked to	*/
/* the ground station.													*/
/* @param: severity: 1 = Normal.										*/
/* @param: report_id: Unique to the event report, ex: BIT_FLIP_DETECTED */
/* @param: param1,0 extra information that can be sent to ground.		*/
/************************************************************************/
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

// This function will kill the scheduling task.
// If it is being called by the sched task 0 is passed, otherwise it is probably the FDIR task and 1 should be passed.
void scheduling_kill(uint8_t killer)
{
	// Free the memory that this task allocated.
	vPortFree(current_command);
	vPortFree(num_commands);
	vPortFree(next_command_time);
	vPortFree(furthest_command_time);
	vPortFree(scheduling_on);
	vPortFree(sched_buff0);
	vPortFree(sched_buff1);
	vPortFree(x);
	// Kill the task.
	if(killer)
		vTaskDelete(scheduling_HANDLE);
	else
		vTaskDelete(NULL);
	return;
}
