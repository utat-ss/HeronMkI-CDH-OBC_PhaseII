/*
Author: Keenan Burnett, Bill Bateman
***********************************************************************
* FILE NAME: fdir.c
*
* PURPOSE:
* This file is to be used to house the FDIR task and related functions.
*	FDIR == "Failure Detection, Isolation, and Recovery"
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
* 11/10/2015		Created.
*
* 12/09/2015		I added in check_error() and decode_error() and am starting to fill in their blank
*					parts with resolution sequences from FDIR.docx in the Owncloud.
*
* 12/12/2015		I added in reset_SSM(), send_Event_Report(), enter_SAFE_MODE(), and am working on filling
*					in more parts to check_error().
*
* 12/13/2015		I'm adding in code for the resolution sequence of ERROR# 1 (lots of this code could be turned into
*					helper functions and reused elsewhere in the decode_error() function.
*
* 12/16/2015		Added the code for resolution sequence #5 which takes care of what happens when a task experiences a faulty
*					fifo.
*
* 12/17/2015		I'm adding in more resolution sequences and am currently working on RS7. I was able to reuse resolutions for 
*					error numbers 6,8,9,10.
*
* 12/18/2015		I'm still working on RS7, I added global variable req_data_timeout so that FDIR could adjust the timeout
*					being used in request_sensor_data_h. I have now completed the resolution sequences for error numbers 11-17
*
* 12/22/2015		Today I have been working on resolution sequences up to 31 (currently the last one).
*
*					Resolution sequences are now complete.
*
*					Started working on exec_commands(), leaving diagnostics for William to work on.
*
* 12/23/2015		exec_commands() is pretty much done, save for the parts involving diagnostics. Need to finish the implementation
*					of FDIR commands to SSMs on the SSM side of things. I also need a confirmation for entering low power mode or
*					coms takeover mode.
*
* 01/05/2016		I am adding in some code for the INTERNAL_MEMORY_FALLBACK mode which is used when all 3 SPI memory chips appear to be dead.		
*	
* 1/15/2016			Bill: Added in diagnostics functions and variables (similar to housekeeping). Put diagnostics code in
*					exec_commands() and enter_SAFE_MODE(). I tried to label all places an error could occur with FAILURE_RECOVERY.
*
* 1/20/2016			Bill: When errors occur in diagnostics, now calls send_event_report(). Defined some errors in global_var.h.
*					Also fixed some wrong variables (still had hk names).
*
* 03/27/2016		K: Added function headers which were missing.
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

/* Atmel library includes. */
#include "asf.h"
/* Common demo includes. */
#include "partest.h"
/*		RTC includes.	*/
#include "rtc.h"
/*		CAN includes	*/
#include "can_func.h"
/*		Global variables used in entire project */
#include "global_var.h"
/*		SPI includes	*/
#include "spimem.h"
/*	All things Error-handling related */
#include "error_handling.h"
/*  Time related includes				*/
#include "time.h"
/*	Watchdog timer includes				*/
#include <asf/sam/drivers/wdt/wdt.h>
/* Includes related to atomic operation	*/
#include "atomic.h"
/* Includes related to SSM reprogramming	*/
#include "ssm_programming.h"
/* Checksum related includes			*/
#include "checksum.h"

/* Priorities at which the tasks are created. */
#define FDIR_PRIORITY		( tskIDLE_PRIORITY + 5 )

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define FDIR_PARAMETER			( 0xABCD )

/* DIAGNOSTICS DEFINITION DEFINES */
#define DEFAULT				0
#define ALTERNATE			1

/* Functions Prototypes. */
static void prvFDIRTask( void *pvParameters );
TaskHandle_t fdir(void);
static void check_error(void);
static void decode_error(uint32_t error, uint8_t severity, uint8_t task, uint8_t code);
static void exec_commands(void);
static void clear_current_command(void);
static void clear_test_arrays(void);
static void send_event_report(uint8_t severity, uint8_t report_id, uint8_t param1, uint8_t param0);
int restart_task(uint8_t task_id, TaskHandle_t task_HANDLE);
static void time_update(void);
static void enter_SAFE_MODE(uint8_t reason);
static void init_vars(void);
void clear_fifo_buffer(void);
int recreate_fifo_h(QueueHandle_t *queue_to_recreate);
int recreate_fifo(uint8_t task_id, uint8_t direction);
static void clear_fdir_signal(uint8_t task);
static int request_enter_low_power_mode(void);
static int request_exit_low_power_mode(void);
static int request_enter_coms_takeover(void);
static int request_exit_coms_takeover(void);
static int request_pause_operations(uint8_t ssmID);
static int request_resume_operations(uint8_t ssmID);
int reset_SSM(uint8_t ssm_id);
static uint8_t get_fdir_signal(uint8_t task);
int delete_task(uint8_t task_id);
static void send_tc_execution_verify(uint8_t status, uint16_t packet_id, uint16_t psc);
void enter_INTERNAL_MEMORY_FALLBACK(void);
void exit_INTERNAL_MEMORY_FALLBACK(void);

/* Prototypes for resolution sequences */
static void resolution_sequence1(uint8_t code, uint8_t task);
static void resolution_sequence1_1(uint8_t task);
static void resolution_sequence1_2(uint8_t task);
static void resolution_sequence1_3(uint8_t task);
static void resolution_sequence1_4(uint8_t task);
static void resolution_sequence4(uint8_t task, uint8_t command);
static void resolution_sequence5(uint8_t task, uint8_t code);
static void resolution_sequence7(uint8_t task, uint8_t parameter);
static void resolution_sequence11(uint8_t task);
static void resolution_sequence14(uint8_t task, uint32_t sect_num, uint8_t chip);
static void resolution_sequence18(uint8_t task);
static void resolution_sequence20(uint8_t task);
static void resolution_sequence25(uint8_t task, uint8_t parameter);
static void resolution_sequence29(uint8_t ssmID);
static void resolution_sequence31(void);

/* Prototypes for diagnostics functions */
static void clear_current_diag(void);
static int request_diagnostics_all(void);
static int store_diagnostics(void);
static void setup_default_definition(void);
static void set_definition(uint8_t sID);
static void clear_alternate_diag_definition(void);
static void send_diag_as_tm(void);
static void send_diag_param_report(void);
static int store_diag_in_spimem(void);
static void set_diag_mem_offset(void);

/* External functions used to create and encapsulate different tasks*/
extern TaskHandle_t housekeep(void);
extern TaskHandle_t time_manage(void);
extern TaskHandle_t eps(void);
extern TaskHandle_t coms(void);
extern TaskHandle_t payload(void);
extern TaskHandle_t memory_manage(void);
extern TaskHandle_t wdt_reset(void);
extern TaskHandle_t obc_packet_router(void);
extern TaskHandle_t scheduling(void);

/* External functions used to kill different tasks	*/
extern void housekeep_kill(uint8_t killer);
extern void scheduling_kill(uint8_t killer);
extern void time_manage_kill(uint8_t killer);
extern void memory_manage_kill(uint8_t killer);
extern void wdt_reset_kill(uint8_t killer);
extern void eps_kill(uint8_t killer);
extern void coms_kill(uint8_t killer);
extern void payload_kill(uint8_t killer);
extern void opr_kill(uint8_t killer);

/* External functions used for time management	*/
extern void broadcast_minute(void);
extern void update_absolute_time(void);
extern void report_time(void);

/* External functions used for OBC variables */
extern void set_obc_variable(uint8_t parameter, uint32_t val);
extern uint32_t get_obc_variable(uint8_t parameter);

/* External functions used for diagnostics */
extern uint8_t get_ssm_id(uint8_t sensor_name);

/* Local Variables for FDIR */
static uint8_t current_command[DATA_LENGTH + 10];	// Used to store commands which are sent from the OBC_PACKET_ROUTER.
static uint8_t test_array1[256], test_array2[256];
static uint32_t minute_count;


/* Local Variables for Diagnostics */
static uint8_t current_diag[DATA_LENGTH];			 //stores the next diagnostics packet to downlink
static uint8_t diag_definition0[DATA_LENGTH];		 //stores default diagnostic definition
static uint8_t diag_definition1[DATA_LENGTH];		 //stores alternate diagnostic definition
static uint8_t diag_updated[DATA_LENGTH];
static uint8_t current_diag_definition[DATA_LENGTH]; //stores current diagnostic definition
static uint8_t current_diag_definitionf;
//static uint8_t current_eps_diag[DATA_LENGTH / 4], current_coms_diag[DATA_LENGTH / 4], current_pay_diag[DATA_LENGTH / 4];
static uint32_t new_diag_msg_high, new_diag_msg_low;
static uint8_t current_diag_fullf, diag_param_report_requiredf;
static uint8_t collection_interval0, collection_interval1;
static uint8_t current_diag_mem_offset[4];

static uint32_t diag_time_to_wait;
static uint32_t diag_last_report_minutes;
static uint32_t diag_num_hours;
static uint32_t diag_old_minute;

/* Fumble Counts */
static uint8_t housekeep_fumble_count;
static uint8_t scheduling_fumble_count;
static uint8_t time_manage_fumble_count;
static uint8_t memory_manage_fumble_count;
static uint8_t wdt_reset_fumble_count;
static uint8_t eps_fumble_count;
static uint8_t coms_fumble_count;
static uint8_t payload_fumble_count;
static uint8_t opr_fumble_count;
static uint8_t eps_SSM_fumble_count;
static uint8_t coms_SSM_fumble_count;
static uint8_t payload_SSM_fumble_count;
static uint8_t SPI_CHIP_1_fumble_count;
static uint8_t SPI_CHIP_2_fumble_count;
static uint8_t SPI_CHIP_3_fumble_count;
static uint8_t hk_fifo_to_OPR_fumble_count;
static uint8_t hk_fifo_from_OPR_fumble_count;
static uint8_t sched_fifo_to_OPR_fumble_count;
static uint8_t sched_fifo_from_OPR_fumble_count;
static uint8_t time_fifo_to_OPR_fumble_count;
static uint8_t time_fifo_from_OPR_fumble_count;
static uint8_t memory_fifo_to_OPR_fumble_count;
static uint8_t memory_fifo_from_OPR_fumble_count;
static uint8_t wdt_fifo_to_OPR_fumble_count;
static uint8_t wdt_fifo_from_OPR_fumble_count;
static uint8_t eps_fifo_to_OPR_fumble_count;
static uint8_t eps_fifo_from_OPR_fumble_count;
static uint8_t coms_fifo_to_OPR_fumble_count;
static uint8_t coms_fifo_from_OPR_fumble_count;
static uint8_t pay_fifo_to_OPR_fumble_count;
static uint8_t pay_fifo_from_OPR_fumble_count;

/* Variables used for Error Handling */
static uint32_t error;
static uint8_t code, task;
static uint32_t timeout;
static uint32_t offset;

/* Variables used for decoding telecommands */
static uint8_t command, service_type, memid, ssmID;
static uint16_t packet_id, psc;
static uint32_t address, length, num_transfers = 0;

/* Reason for entering SAFE_MODE */
static uint8_t SMERROR;

/* Time variable to use during SAFE_MODE */
static struct timestamp time;

/************************************************************************/
/* FDIR (Function)														*/
/* @Purpose: This function is used to create the fdir task.				*/
/************************************************************************/
TaskHandle_t fdir( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		TaskHandle_t temp_HANDLE;
		xTaskCreate( prvFDIRTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) FDIR_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					FDIR_PRIORITY, 			/* The priority assigned to the task. */
					&temp_HANDLE );								/* The task handle is not required, so NULL is passed. */
	return temp_HANDLE;
}
/*-----------------------------------------------------------*/

/************************************************************************/
/*				FDIR TASK												*/
/*	This task receives commands from obc_packet_router as well as		*/
/* failure messages from other tasks. It then proceeds to attempt to	*/
/* resolve the issue as best it can.									*/
/************************************************************************/
static void prvFDIRTask( void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == FDIR_PARAMETER );
	// Initialize all variables which are local to FDIR
	init_vars();

	/* @non-terminating@ */	
	for( ;; )
	{
		check_error();
		exec_commands();
	}
}
/*-----------------------------------------------------------*/

// Static functions defined below.

/************************************************************************/
/* FLETCHER16				                                            */
/* @Purpose: This function shall be used to check if there is any		*/
/* required action for the FDIR task to deal with by reading both the	*/
/* high_sev fifo and the low_sev fifo.									*/
/************************************************************************/
static void check_error(void)
{
	error = 0;
	code = 0;
	task = 0;
	// Check if there is a high-severity error to deal with.
	if (xSemaphoreTake(Highsev_Mutex, (TickType_t)1) == pdTRUE)
	{
		if( xQueueReceive(high_sev_to_fdir_fifo, current_command, (TickType_t)1) == pdTRUE)
		{
			error = ((uint32_t)(current_command[151])) << 24;
			error += ((uint32_t)current_command[150]) << 16;
			error += ((uint32_t)current_command[149]) << 8;
			error += ((uint32_t)current_command[148]);
			task = current_command[147];
			code = current_command[146];
			decode_error(error, 1, task, code);
		}
		xSemaphoreGive(Highsev_Mutex);
	}
	// Check if there is a low-severity error to deal with.
	if (xSemaphoreTake(Lowsev_Mutex, (TickType_t)1) == pdTRUE)
	{
		if( xQueueReceive(low_sev_to_fdir_fifo, current_command, (TickType_t)1) == pdTRUE)
		{
			error = ((uint32_t)(current_command[151])) << 24;
			error += ((uint32_t)current_command[150]) << 16;
			error += ((uint32_t)current_command[149]) << 8;
			error += ((uint32_t)current_command[148]);
			decode_error(error, 2, task, code);
		}
		xSemaphoreGive(Highsev_Mutex);	
	}

	return;
}

/************************************************************************/
/* FLETCHER16				                                            */
/* @Purpose: Depending on (error), the function will run different		*/
/* resolution sequences which are denoted by different functions.		*/
/* @param: error: (code from error_handling.h)							*/
/* @param: task: ID of the task which experienced the error ex:			*/
/* @param: code: Extra information required for resolution				*/
/* (see implementation)													*/
/* HK_TASK_ID															*/
/* @Note: Both high and low severity errors can be sent to decode_error	*/
/************************************************************************/
static void decode_error(uint32_t error, uint8_t severity, uint8_t task, uint8_t code)
{
	// This is where the resolution sequences are going to go.
	uint8_t chip, ssmID;
	uint32_t sect_num = 0xFFFFFFFF;
	if(severity)
	{
		switch(error)
		{
			case 1:
				if(task != SCHEDULING_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1(code, task);
			case 2:
				if(task != SCHEDULING_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1(code, task);
			case 3:
				if(task != SCHEDULING_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1(code, task);
			case 4:
				if(task != SCHEDULING_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence4(task, code);	// code here should be the command that failed.
			case 5:
				if(task != SCHEDULING_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence5(task, code);			// code: 0 = from OPR, 1 = to OPR
			case 6:
				if(task != HK_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence5(task, code);
			case 7:
				if(task != HK_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence7(task, code);			// Here code is ID for the parameter that failed.
			case 8:
				if(task != HK_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1(code, task);
			case 0x1C:
				if(task != HK_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1(code, task);
			case 9:
				if(task != TIME_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence5(task, code);
			case 10:
				enter_INTERNAL_MEMORY_FALLBACK();
				enter_SAFE_MODE(SPIMEM_ERROR_DURING_INIT);
			case 11:
				resolution_sequence11(task);
			case 12:
				resolution_sequence1_4(task);
			case 13:
				resolution_sequence1_4(task);
			case 14:
				sect_num = ((uint32_t)current_command[146]) << 24;
				sect_num += ((uint32_t)current_command[145]) << 16;
				sect_num += ((uint32_t)current_command[144]) << 8;
				sect_num += (uint32_t)current_command[143];
				chip = current_command[142];
				resolution_sequence14(task, sect_num, chip);
			case 15:
				resolution_sequence1_4(task);
			case 16:
				resolution_sequence1_4(task);
			case 17:
				enter_INTERNAL_MEMORY_FALLBACK();
				clear_fdir_signal(task);		
			case 18:
				resolution_sequence18(task);
			case 19:
				if(task != MEMORY_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				enter_INTERNAL_MEMORY_FALLBACK();
				clear_fdir_signal(task);
			case 20:
				if(task != MEMORY_TASK_ID)
				enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence20(task);
			case 21:
				if(task != MEMORY_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1_4(task);
			case 22:
				if(task != MEMORY_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence5(task, code);
			case 23:
				if(task != EPS_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence7(task, code);
			case 24:
				if(task != EPS_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence7(task, code);
			case 25:
				if(task != OBC_PACKET_ROUTER_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence25(task, code);
			case 29:
				if(task != OBC_PACKET_ROUTER_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				ssmID = current_command[147];
				resolution_sequence29(ssmID);
			case 31:
				if(task != OBC_PACKET_ROUTER_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence31();
			case 32:
				if(task != PAY_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1(code, task);
			case 33:
				if(task != EPS_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence5(task, code);
			default:
				enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);			
		}
	}
	return;
}

/************************************************************************/
/* RESOLUTION SEQUENCES		                                            */
/* @Purpose: Each of the following function corresponds to the resolutio*/
/* of a particular error. See FDIR.docx for more information.			*/
/* @Note: If an error is resolved, the FDIR signal shall be cleared		*/
/************************************************************************/
static void resolution_sequence1(uint8_t code, uint8_t task)
{
	if(code == 0xFF)				// All SPI Memory chips are dead.
		resolution_sequence1_1(task);
	if(code == 0xFE)				// Usage error on the part of the task that called the function.
		resolution_sequence1_2(task);
	if(code == 0xFD)				// Spi0_Mutex was not free when this task was called.
		resolution_sequence1_3(task);
	if(code == 0xFC)				// The chip was not "ready for command" during SPI operation.
		resolution_sequence1_4(task);
	return;
}

static void resolution_sequence1_1(uint8_t task)
{
	enter_INTERNAL_MEMORY_FALLBACK();
	clear_fdir_signal(task);
	return;
}

static void resolution_sequence1_2(uint8_t task)
{					
	// The calling task was used incorrectly.
	scheduling_fumble_count++;
	if(scheduling_fumble_count == 10)
		restart_task(SCHEDULING_TASK_ID, (TaskHandle_t)0);
	if(scheduling_fumble_count > 10)
		enter_SAFE_MODE(SCHEDULING_MALFUNCTION);
	clear_fdir_signal(task);
	return;
}

static void resolution_sequence1_3(uint8_t task)
{
	TaskHandle_t temp_task = 0;
	eTaskState task_state = 0;
	uint8_t i;
	delay_ms(50);				// Give the currently running operation time to finish.
	if (xSemaphoreTake(Spi0_Mutex, (TickType_t)5000) == pdTRUE)	// Wait for a maximum of 5 seconds.
	{
		clear_fdir_signal(task);
		xSemaphoreGive(Spi0_Mutex);		// Nothing seems to be wrong.
		return;
	}
	// The Spi0_Mutex is still held by another task.
	temp_task = xSemaphoreGetMutexHolder(Spi0_Mutex);		// Get the task which currently holds this mutex.
	task_state = eTaskGetState(temp_task);
	if(task_state < 2) // Task is in an operational state,
	{
		vTaskSuspend(temp_task);	// Suspend the task from running further.
	}
	enter_atomic();		// CRITICAL SECTION
	restart_task(temp_task, (TaskHandle_t)0);
	Spi0_Mutex = xSemaphoreCreateBinary();
	xSemaphoreGive(Spi0_Mutex);
	exit_atomic();		// END CRITICAL SECTION
	for (i = 0; i < 50; i++)
	{
		taskYIELD();				// Give the new task ample time to start running again.
	}
	// Try to acquire the mutex again.
	if (xSemaphoreTake(Spi0_Mutex, (TickType_t)5000) == pdTRUE)	// Wait for a maximum of 5 seconds.
	{
		clear_fdir_signal(task);
		xSemaphoreGive(Spi0_Mutex);		// Nothing seems to be wrong.
		return;
	}
	// Something has gone wrong, enter SAFE_MODE and let ground know.
	enter_SAFE_MODE(SPI0_MUTEX_MALFUNCTION);
	return;
}

static void resolution_sequence1_4(uint8_t task)
{					
	uint16_t i;
	if(SPI_HEALTH1)
	{
		SPI_CHIP_1_fumble_count++;
		if(SPI_CHIP_1_fumble_count >= 10)
		SPI_HEALTH1 = 0;		// Mark this chip as dysfunctional.
	}
	else if(SPI_HEALTH2)
	{
		SPI_CHIP_2_fumble_count++;
		if(SPI_CHIP_2_fumble_count >= 10)
		SPI_HEALTH2 = 0;		// Mark this chip as dysfunctional.
	}
	else if(SPI_HEALTH3)
	{
		SPI_CHIP_3_fumble_count++;
		if(SPI_CHIP_3_fumble_count >= 10)
		SPI_HEALTH3 = 0;		// Mark this chip as dysfunctional.
		enter_INTERNAL_MEMORY_FALLBACK();
		clear_fdir_signal(task);	// Issue resolved.
		return;
	}
	// At least one of the chips is working or has not fumbled too many times.
	clear_test_arrays();
	for(i = 0; i < 256; i++)
	{
		test_array1[(uint8_t)i] = (uint8_t)i;
	}
	
	// Check to see if reading/writing is possible from the chip which malfunctioned.
	if(SPI_HEALTH1)
	{
		if( spimem_write(0x00, test_array1, 256) < 0)
		{
			enter_INTERNAL_MEMORY_FALLBACK();
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		if( spimem_read(0x00, test_array2, 256) < 0)
		{
			enter_INTERNAL_MEMORY_FALLBACK();
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		for(i = 0; i < 256; i++)
		{
			if(test_array2[(uint8_t)i] != test_array1[(uint8_t)i])
			SPI_HEALTH1 = 0;
		}
		if(SPI_HEALTH1)
		{
			clear_fdir_signal(task);	// The issue was only temporary.
			return;
		}
	}
	else if(SPI_HEALTH2)			// Try switching to a different chip
	{
		if( spimem_write(0x00, test_array1, 256) < 0)
		{
			enter_INTERNAL_MEMORY_FALLBACK();
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		spimem_read(0x00, test_array2, 256);
		{
			enter_INTERNAL_MEMORY_FALLBACK();
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		for(i = 0; i < 256; i++)
		{
			if(test_array2[(uint8_t)i] != test_array1[(uint8_t)i])
			SPI_HEALTH2 = 0;	// Try with a different chip.
		}
		if(SPI_HEALTH2)
		{
			clear_fdir_signal(task);	// The issue was only temporary.
			return;
		}
	}
	else if(SPI_HEALTH3)
	{
		if( spimem_write(0x00, test_array1, 256) < 0)
		{
			enter_INTERNAL_MEMORY_FALLBACK();
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		spimem_read(0x00, test_array2, 256);
		{
			enter_INTERNAL_MEMORY_FALLBACK();
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		for(i = 0; i < 256; i++)
		{
			if(test_array2[(uint8_t)i] != test_array1[(uint8_t)i])
			SPI_HEALTH3 = 0;	// Try with a different chip.
		}
		if(SPI_HEALTH3)
		{
			clear_fdir_signal(task);	// The issue was only temporary.
			return;
		}
	}
	// There are no functional chips, INTERNAL MEMORY FALLBACK MODE
	// TODO: should enter_INTERNAL_MEMORY_FALLBACK() be called here
	clear_fdir_signal(task);
	return;
}

// command - Currently corresponds to the K-service command which was scheduled and failed. 
static void resolution_sequence4(uint8_t task, uint8_t command)
{
	restart_task(task, (TaskHandle_t)0);
	// Retry the command which failed. (should be located in current_command[])
	// Play with the SSM if one was involved.
	// If the command worked now: return that the issue was resolved.
	
	// Pause Scheduled operations:
	scheduling_on = 0;
	enter_SAFE_MODE(SCHED_COMMAND_FAILED);
	return;
}

static void resolution_sequence5(uint8_t task, uint8_t code)
{
	/*
     * Read/Write from a FIFO failed.
	 * Steps:
	 *			1. Increment a counter corresponding to this fifo failing.
	 *				a. If the counter is low, proceed to 2.
	 *				b. If the counter gets too high, attempt to reset the calling task.
	 *			2. Try deleting and recreating the FIFO.
	 */
	switch(task)
	{
		case HK_TASK_ID:
			if(code)
				hk_fifo_to_OPR_fumble_count++;
			else
				hk_fifo_from_OPR_fumble_count++;
			// If the fumble count gets too high, restart the task which might be faulty.
			if((hk_fifo_to_OPR_fumble_count == 5) || (hk_fifo_from_OPR_fumble_count == 5))
			{
				restart_task(HK_TASK_ID, (TaskHandle_t)0);
				hk_fdir_signal = 0;
				return;
			}
			if((hk_fifo_to_OPR_fumble_count > 5) || (hk_fifo_from_OPR_fumble_count > 5))
			{
				enter_SAFE_MODE(DYSFUNCTIONAL_FIFO);
				hk_fdir_signal = 0;
				return;
			}
			if((hk_fifo_to_OPR_fumble_count < 5) && (hk_fifo_from_OPR_fumble_count < 5))
			{
				recreate_fifo(HK_TASK_ID, code);
				hk_fdir_signal = 0;
				return;
			}			
		case TIME_TASK_ID:
			if(code)
				time_fifo_to_OPR_fumble_count++;
			else
				time_fifo_from_OPR_fumble_count++;
			// If the fumble count gets too high, restart the task which might be faulty.
			if((time_fifo_to_OPR_fumble_count == 5) || (time_fifo_from_OPR_fumble_count == 5))
			{
				restart_task(TIME_TASK_ID, (TaskHandle_t)0);
				time_fdir_signal = 0;
				return;
			}
			if((time_fifo_to_OPR_fumble_count > 5) || (time_fifo_from_OPR_fumble_count > 5))
			{
				enter_SAFE_MODE(DYSFUNCTIONAL_FIFO);
				time_fdir_signal = 0;
				return;
			}
			if((time_fifo_to_OPR_fumble_count < 5) && (time_fifo_from_OPR_fumble_count < 5))
			{
				recreate_fifo(TIME_TASK_ID, code);
				time_fdir_signal = 0;
				return;
			}
		case COMS_TASK_ID:
			if(code)
				coms_fifo_to_OPR_fumble_count++;
			else
				coms_fifo_from_OPR_fumble_count++;
			// Just try resetting the task.
			if((coms_fifo_to_OPR_fumble_count == 1) || (coms_fifo_from_OPR_fumble_count == 1))
			{
				restart_task(COMS_TASK_ID, (TaskHandle_t)0);
				coms_fdir_signal = 0;
				return;
			}
			// If that didn't work, enter into SAFE_MODE.
			if((coms_fifo_to_OPR_fumble_count > 1) || (coms_fifo_from_OPR_fumble_count > 1))
			{
				enter_SAFE_MODE(IMPORTANT_FIFO_FAILED);
				coms_fdir_signal = 0;
				return;
			}
		case EPS_TASK_ID:
			if(code)
				eps_fifo_to_OPR_fumble_count++;
			else
				eps_fifo_from_OPR_fumble_count++;
			// Just try resetting the task.
			if((eps_fifo_to_OPR_fumble_count == 1) || (eps_fifo_from_OPR_fumble_count == 1))
			{
				restart_task(EPS_TASK_ID, (TaskHandle_t)0);
				eps_fdir_signal = 0;
				return;
			}
			// If that didn't work, enter into SAFE_MODE.
			if((eps_fifo_to_OPR_fumble_count > 1) || (eps_fifo_from_OPR_fumble_count > 1))
			{
				enter_SAFE_MODE(IMPORTANT_FIFO_FAILED);
				eps_fdir_signal = 0;
				return;
			}
		case PAY_TASK_ID:
			if(code)
				pay_fifo_to_OPR_fumble_count++;
			else
				pay_fifo_from_OPR_fumble_count++;
			// Just try resetting the task.
			if((pay_fifo_to_OPR_fumble_count == 1) || (pay_fifo_from_OPR_fumble_count == 1))
			{
				restart_task(PAY_TASK_ID, (TaskHandle_t)0);
				pay_fdir_signal = 0;
				return;
			}
			// If that didn't work, enter into SAFE_MODE.
			if((pay_fifo_to_OPR_fumble_count > 1) || (pay_fifo_from_OPR_fumble_count > 1))
			{
				enter_SAFE_MODE(IMPORTANT_FIFO_FAILED);
				pay_fdir_signal = 0;
				return;
			}
		case OBC_PACKET_ROUTER_ID:
			// Just try resetting the task
			opr_fumble_count++;
			if(opr_fumble_count == 1)
			{
				restart_task(OBC_PACKET_ROUTER_ID, (TaskHandle_t)0);
				opr_fdir_signal = 0;
				return;				
			}
			if(opr_fumble_count > 1)
			{
				enter_SAFE_MODE(IMPORTANT_FIFO_FAILED);
				opr_fdir_signal = 0;
				return;				
			}
		case SCHEDULING_TASK_ID:
			if(code)
				sched_fifo_to_OPR_fumble_count++;
			else
				sched_fifo_from_OPR_fumble_count++;
			// If the fumble count gets too high, restart the task which might be faulty.
			if((sched_fifo_to_OPR_fumble_count == 5) || (sched_fifo_from_OPR_fumble_count == 5))
			{
				restart_task(SCHEDULING_TASK_ID, (TaskHandle_t)0);
				sched_fdir_signal = 0;
				return;
			}
			if((sched_fifo_to_OPR_fumble_count > 5) || (sched_fifo_from_OPR_fumble_count > 5))
			{
				enter_SAFE_MODE(DYSFUNCTIONAL_FIFO);
				sched_fdir_signal = 0;
				return;
			}
			if((sched_fifo_to_OPR_fumble_count < 5) && (sched_fifo_from_OPR_fumble_count < 5))
			{
				recreate_fifo(SCHEDULING_TASK_ID, code);
				sched_fdir_signal = 0;
				return;
			}
		case MEMORY_TASK_ID:
			if(code)
				memory_fifo_to_OPR_fumble_count++;
			else
				memory_fifo_from_OPR_fumble_count++;
			// If the fumble count gets too high, restart the task which might be faulty.
			if((memory_fifo_to_OPR_fumble_count == 5) || (memory_fifo_from_OPR_fumble_count == 5))
			{
				restart_task(HK_TASK_ID, (TaskHandle_t)0);
				mem_fdir_signal = 0;
				return;
			}
			if((memory_fifo_to_OPR_fumble_count > 5) || (memory_fifo_from_OPR_fumble_count > 5))
			{
				enter_SAFE_MODE(DYSFUNCTIONAL_FIFO);
				mem_fdir_signal = 0;
				return;
			}
			if((memory_fifo_to_OPR_fumble_count < 5) && (memory_fifo_from_OPR_fumble_count < 5))
			{
				recreate_fifo(HK_TASK_ID, code);
				mem_fdir_signal = 0;
				return;
			}
		default:
			enter_SAFE_MODE(ERROR_IN_RS5);
		return;		// SAFE_MODE ?
	}
	return;
}

static void resolution_sequence7(uint8_t task, uint8_t parameter)
{
	uint8_t ssmID = 0xFF;
	int *status = 0;
	ssmID = get_ssm_id(parameter);
	// If the parameter is internal to the OBC, enter SAFE_MODE.
	if(ssmID == OBC_ID)
	{
		enter_SAFE_MODE(OBC_PARAM_FAILED);
		return;
	}
	// Otherwise, increase the timeout and try again.
	req_data_timeout += 2000000;		// Add 25 ms to the REQ_DATA timeout. (See can_func.c >> request_sensor_data_h() )
	if(req_data_timeout > 10000000)
		enter_SAFE_MODE(REQ_DATA_TIMEOUT_TOO_LONG);		// If the timeout gets too long, we enter into SAFE_MODE.
	request_sensor_data(FDIR_TASK_ID, ssmID, parameter, status);
	if(*status > 0)
	{
		clear_fdir_signal(task);	// The issue was resolved.
		return;
	}
	// Try resetting the SSM which has the malfunctioning variable and attempt to acquire the parameter again.
	reset_SSM(ssmID);
	request_sensor_data(FDIR_TASK_ID, ssmID, parameter, status);
	if(*status > 0)
	{
		clear_fdir_signal(task);	// The issue was resolved.
		return;
	}
	// Try reprogramming the SSM and attempt to acquire the parameter again.
	reprogram_ssm(ssmID);
	request_sensor_data(FDIR_TASK_ID, ssmID, parameter, status);
	if(*status > 0)
	{
		clear_fdir_signal(task);	// The issue was resolved.
		return;
	}
	enter_SAFE_MODE(SSM_PARAM_FAILED);
	clear_fdir_signal(task);
	return;
}

static void resolution_sequence11(uint8_t task)
{
	chip_erase_timeout += 100;		// Increase the timeout by 1s.
	if(chip_erase_timeout > 3000)	// 30s timeout.
	{
		enter_SAFE_MODE(ER_SEC_TIMEOUT_TOO_LONG);
		clear_fdir_signal(task);
		return;				
	}
	if(erase_spimem() > 0)
	{
		clear_fdir_signal(task);
		return;		
	}
	enter_SAFE_MODE(SPIMEM_INIT_FAILED);
	return;
}

// Address - location of the sector which took too long to erase.
static void resolution_sequence14(uint8_t task, uint32_t sect_num, uint8_t chip)
{
	erase_sector_timeout += 10;		// Increase the timeout by 100ms.
	if (erase_sector_timeout == 100)
	{
		enter_SAFE_MODE(ER_SEC_TIMEOUT_TOO_LONG);
		clear_fdir_signal(task);
		return;
	}
	if (erase_sector_on_chip(chip, sect_num) > 0)
	{
		clear_fdir_signal(task);
		return;		
	}
	// The chip is being buggy, follow the normal resolution sequence for this.
	resolution_sequence1_4(task);
	return;
}

static void resolution_sequence18(uint8_t task)
{
	uint8_t status = 0;
	// The chip is being buggy, attempt to repair it.
	resolution_sequence1_4(task);
	status = get_fdir_signal(task);
	// If the error still hasn't been resolve, enter SAFE_MODE.
	if(status)
		enter_SAFE_MODE(SPIMEM_FAIL_IN_RTC_INIT);
	clear_fdir_signal(task);
	return;
}

static void resolution_sequence20(uint8_t task)
{
	uint8_t status;
	// The chip is being buggy, attempt to repair it.
	resolution_sequence1_4(task);
	status = get_fdir_signal(task);
	// If the error stil hasn't been resolve, enter SAFE_MODE.
	if(status)
	enter_SAFE_MODE(SPIMEM_FAIL_IN_MEM_WASH);
	clear_fdir_signal(task);
	return;	
}

static void resolution_sequence25(uint8_t task, uint8_t parameter)
{
	uint8_t ssmID = COMS_ID;
	int* status = 0;
	
	//ssmID = get_ssm_id(parameter);
	// Otherwise, increase the timeout and try again.
	req_data_timeout += 2000000;		// Add 25 ms to the REQ_DATA timeout. (See can_func.c >> request_sensor_data_h() )
	if(req_data_timeout > 10000000)
		enter_SAFE_MODE(REQ_DATA_TIMEOUT_TOO_LONG);		// If the timeout gets too long, we enter into SAFE_MODE.
	request_sensor_data(FDIR_TASK_ID, ssmID, parameter, status);
	if(*status > 0)
	{
		clear_fdir_signal(task);	// The issue was resolved.
		return;
	}
	// Try resetting the SSM which has the malfunctioning variable and attempt to acquire the parameter again.
	reset_SSM(ssmID);
	request_sensor_data(FDIR_TASK_ID, ssmID, parameter, status);
	if(*status > 0)
	{
		clear_fdir_signal(task);	// The issue was resolved.
		return;
	}
	// Try reprogramming the SSM and attempt to acquire the parameter again.
	reprogram_ssm(ssmID);
	request_sensor_data(FDIR_TASK_ID, ssmID, parameter, status);
	if(*status > 0)
	{
		clear_fdir_signal(task);	// The issue was resolved.
		return;
	}
	enter_SAFE_MODE(SSM_PARAM_FAILED);
	clear_fdir_signal(task);
	return;
}

static void resolution_sequence29(uint8_t ssmID)
{
	ssm_consec_trans_timeout = 0;
	int status = 0;
	ssm_consec_trans_timeout = request_sensor_data(FDIR_TASK_ID, COMS_ID, SSM_CTT, &status);
	ssm_consec_trans_timeout += 10;		// Increase the timeout by 1ms.

	if(ssm_consec_trans_timeout < 250)
		set_variable(FDIR_TASK_ID, COMS_ID, SSM_CTT, ssm_consec_trans_timeout);
	
	if(ssm_consec_trans_timeout == 230)			// Try resetting the SSM and hope that this resolves the issue.
		reset_SSM(ssmID);
	if(ssm_consec_trans_timeout == 240)			// Try reprogramming the SSm and hope that this resolves the issue.
		reprogram_ssm(ssmID);
	if (ssm_consec_trans_timeout > 240)			// If the timeout gets too long, we enter into SAFE_MODE.
		enter_SAFE_MODE(SSM_CTT_TOO_LONG);

	if(!ssmID)
		set_variable(FDIR_TASK_ID, COMS_ID, COMS_FDIR_SIGNAL, 0);
	if(ssmID == 1)
		set_variable(FDIR_TASK_ID, EPS_ID, EPS_FDIR_SIGNAL, 0);
	if(ssmID == 2)
		set_variable(FDIR_TASK_ID, PAY_ID, PAY_FDIR_SIGNAL, 0);
	return;
}

static void resolution_sequence31(void)
{
	obc_consec_trans_timeout += 10;		// Increase the timeout by 10ms.
	if(obc_consec_trans_timeout > 240)
		enter_SAFE_MODE(OBC_CTT_TOO_LONG);
	else
		opr_fdir_signal = 0;
	return;
}

/************************************************************************/
/* FLETCHER16				                                            */
/* @Purpose: Clears the fdir signal corresponding to (task)				*/
/* @param: task: ID of the task which you would like to clear the FDIR	*/
/* signal for.															*/
/************************************************************************/
static void clear_fdir_signal(uint8_t task)
{
	switch(task)
	{
		case HK_TASK_ID:
			hk_fdir_signal = 0;
		case TIME_TASK_ID:
			time_fdir_signal = 0;
		case COMS_TASK_ID:
			coms_fdir_signal = 0;
		case EPS_TASK_ID:
			eps_fdir_signal = 0;
		case PAY_TASK_ID:
			pay_fdir_signal = 0;
		case OBC_PACKET_ROUTER_ID:
			opr_fdir_signal = 0;
		case SCHEDULING_TASK_ID:
			sched_fdir_signal = 0;
		case WD_RESET_TASK_ID:
			wdt_fdir_signal = 0;
		case MEMORY_TASK_ID:
			mem_fdir_signal = 0;
		default:
			enter_SAFE_MODE(ERROR_IN_CFS);
	}
	return;
}

/************************************************************************/
/* FLETCHER16				                                            */
/* @Purpose: Returns the state of the fdir signal corresonding to (task)*/
/* @param: task: ex: HK_TASK_ID											*/
/* @return: The state of the signal (0 or 1)							*/
/************************************************************************/
static uint8_t get_fdir_signal(uint8_t task)
{
	switch(task)
	{
		case HK_TASK_ID:
			return hk_fdir_signal;
		case TIME_TASK_ID:
			return time_fdir_signal;
		case COMS_TASK_ID:
			return coms_fdir_signal;
		case EPS_TASK_ID:
			return eps_fdir_signal;
		case PAY_TASK_ID:
			return pay_fdir_signal;
		case OBC_PACKET_ROUTER_ID:
			return opr_fdir_signal;
		case SCHEDULING_TASK_ID:
			return sched_fdir_signal;
		case WD_RESET_TASK_ID:
			return wdt_fdir_signal;
		case MEMORY_TASK_ID:
			return mem_fdir_signal;
		default:
			enter_SAFE_MODE(ERROR_IN_GFS);
	}
	return 0xFF;
}

/************************************************************************/
/* EXEC_COMMANDS		                                                */
/* @Purpose: This function checks whether housekeeping or FDIR have sent*/
/* any commands that the FDIR task needs to act upon.					*/
/************************************************************************/
static void exec_commands(void)
{
	uint8_t i, status, j;
	num_transfers = 0;
	uint32_t val;
	uint8_t* mem_ptr = 0;
	clear_current_command();
	if(xQueueReceive(obc_to_fdir_fifo, current_command, (TickType_t)100) == pdTRUE)
	{
		packet_id = ((uint16_t)current_command[140]) << 8;
		packet_id += (uint16_t)current_command[139];
		psc = ((uint16_t)current_command[138]) << 8;
		psc += (uint16_t)current_command[137];
		service_type = current_command[146];
		command = current_command[145];
		memid = current_command[136];
		uint32_t temp_address;
		int check = 0;
		address =  ((uint32_t)current_command[135]) << 24;
		address += ((uint32_t)current_command[134]) << 16;
		address += ((uint32_t)current_command[133]) << 8;
		address += (uint32_t)current_command[132];
		length =  ((uint32_t)current_command[131]) << 24;
		length += ((uint32_t)current_command[130]) << 16;
		length += ((uint32_t)current_command[129]) << 8;
		length += (uint32_t)current_command[128];
		if(service_type == HK_SERVICE)
		{ // Diagnostics commands
			switch(command)
			{
				case	NEW_DIAG_DEFINITION:
					//create a new diagnostics definition and switch to the new definition
					collection_interval1 = current_command[145];
					for(i = 0; i < DATA_LENGTH; i++)
					{
						diag_definition1[i] = current_command[i];
					}
					diag_definition1[136] = 1;		//sID = 1
					diag_definition1[135] = collection_interval1;
					diag_definition1[134] = current_command[146];
					set_definition(ALTERNATE);
					send_tc_execution_verify(1, packet_id, psc);		// Send TC Execution Verification (Success)
					break;
				case	CLEAR_DIAG_DEFINITION:
					//clear the alternate definition and return to using the default definition
					collection_interval1 = 30; //default interval
					for(i = 0; i < DATA_LENGTH; i++)
					{
						diag_definition1[i] = 0;
					}
					set_definition(DEFAULT);
					send_tc_execution_verify(1, packet_id, psc);
					break;
				case	ENABLE_D_PARAM_REPORT:
					//when reporting diagnostics data, also report the current definition
					diag_param_report_requiredf = 1;
					send_tc_execution_verify(1, packet_id, psc);
					break;
				case	DISABLE_D_PARAM_REPORT:
					//stop reporting the current definition
					diag_param_report_requiredf = 0;
					send_tc_execution_verify(1, packet_id, psc);
					break;
				case	REPORT_DIAG_DEFINITIONS:
					//same as enabled param report
					diag_param_report_requiredf = 1;
					send_tc_execution_verify(1, packet_id, psc);				
					break;
				default:
					break;
			}
		}
		if(service_type == FDIR_SERVICE)
		{
			switch(command)
			{
				case	ENTER_LOW_POWER_MODE:
					if(request_enter_low_power_mode() > 0)
						send_tc_execution_verify(1, packet_id, psc);		// Send TC Execution Verification (Success)
					else
						send_tc_execution_verify(0xFF, packet_id, psc);		// Send TC Execution Verification (Failure)						
				case	EXIT_LOW_POWER_MODE:
					if(request_exit_low_power_mode() > 0)
						send_tc_execution_verify(1, packet_id, psc);
					else
						send_tc_execution_verify(0xFF, packet_id, psc);				
				case	ENTER_SAFE_MODE:
					enter_SAFE_MODE(0);
					send_tc_execution_verify(1, packet_id, psc);
				case	EXIT_SAFE_MODE:
					SAFE_MODE = 0;
					send_tc_execution_verify(1, packet_id, psc);
					send_event_report(1, SAFE_MODE_EXITED, 0, 0);
				case	ENTER_COMS_TAKEOVER_MODE:
					if(request_enter_coms_takeover() > 0)
						send_tc_execution_verify(1, packet_id, psc);
					else
						send_tc_execution_verify(0xFF, packet_id, psc);				
				case	EXIT_COMS_TAKEOVER_MODE:
					if(request_exit_coms_takeover() > 0)
						send_tc_execution_verify(1, packet_id, psc);
					else
						send_tc_execution_verify(0xFF, packet_id, psc);					
				case	PAUSE_SSM_OPERATIONS:
					if(request_pause_operations(current_command[144]) > 0)
						send_tc_execution_verify(1, packet_id, psc);
					else
						send_tc_execution_verify(0xFF, packet_id, psc);					
				case	RESUME_SSM_OPERATIONS:
					if(request_resume_operations(current_command[144]) > 0)
						send_tc_execution_verify(1, packet_id, psc);
					else
						send_tc_execution_verify(0xFF, packet_id, psc);
				case	REPROGRAM_SSM:
					reprogram_ssm(current_command[144]);
				case	RESET_SSM:
					reset_SSM(current_command[144]);
				case	RESET_TASK:
					restart_task(current_command[144], (TaskHandle_t)0);
				case	DELETE_TASK:
					delete_task(current_command[144]);
				case	13:
					ssmID = get_ssm_id(current_command[136]);
					val = (uint32_t)current_command[132];
					val += ((uint32_t)current_command[133]) << 8;
					val += ((uint32_t)current_command[134]) << 16;
					val += ((uint32_t)current_command[135]) << 24;
					if(ssmID < 3)
						set_variable(OBC_PACKET_ROUTER_ID, ssmID, current_command[136], (uint16_t)val);
					else
						set_obc_variable(current_command[136], val);
					send_tc_execution_verify(1, packet_id, psc);			
				case	14:
					ssmID = get_ssm_id(current_command[136]);
					val = (uint32_t)current_command[132];
					val += ((uint32_t)current_command[133]) << 8;
					val += ((uint32_t)current_command[134]) << 16;
					val += ((uint32_t)current_command[135]) << 24;
					if(ssmID < 3)
						val = request_sensor_data(OBC_PACKET_ROUTER_ID, ssmID, current_command[136], status);
					else
						val = get_obc_variable(current_command[136]);
					send_tc_execution_verify(1, packet_id, psc);
					i = current_command[136];
					clear_current_command();
					current_command[146] = SINGLE_PARAMETER_REPORT;
					current_command[136] = i;
					current_command[132] = (uint8_t)val;
					current_command[133] = (uint8_t)(val >> 8);
					current_command[134] = (uint8_t)(val >> 16);
					current_command[135] = (uint8_t)(val >> 24);
					xQueueSendToBack(fdir_to_obc_fifo, current_command, (TickType_t)1);
				default:
					break;
			}
		}
		if(service_type == MEMORY_SERVICE)
		{
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
						check = spimem_write(address, current_command, length);
						if (check <0)
						{
							send_tc_execution_verify(0xFF, packet_id, psc);
							return;
						}
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
							check = spimem_read(address, current_command, length);
							if (check<0)
							{
								send_tc_execution_verify(0xFF, packet_id, psc);
								return;
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
		}
		return;
	}
	else										//FAILURE_RECOVERY
		return;
}

/************************************************************************/
/* REQUEST_ENTER_LOW_POWER_MODE	                                        */
/* @Purpose: Sends a CAN command to the EPS SSM to request that we		*/
/* enter into LOW POWER MODE.											*/
/************************************************************************/
static int request_enter_low_power_mode(void)
{
	timeout = 100;
	if(send_can_command(0, 0, FDIR_TASK_ID, EPS_ID, ENTER_LOW_POWER_COM, DEF_PRIO) < 0)
		enter_SAFE_MODE(CAN_ERROR_WITHIN_FDIR);
	while(!LOW_POWER_MODE && timeout--)
	{
		taskYIELD();		// Wait <= 100ms for EPS SSM to assert LOW_POWER_MODE.
	}
	if(LOW_POWER_MODE)
		return 1;
	return -1;
}

/************************************************************************/
/* REQUEST_EXIT_LOW_POWER_MODE	                                        */
/* @Purpose: Sends a CAN command to the EPS SSM to request that we		*/
/* exit LOW POWER MODE.													*/
/************************************************************************/
static int request_exit_low_power_mode(void)
{
	timeout = 100;
	if(send_can_command(0, 0, FDIR_TASK_ID, EPS_ID, EXIT_LOW_POWER_COM, DEF_PRIO) < 0)
		enter_SAFE_MODE(CAN_ERROR_WITHIN_FDIR);
	while(LOW_POWER_MODE && timeout--)
	{
		taskYIELD();		// Wait <= 100ms for EPS SSM to deassert LOW_POWER_MODE.
	}
	if(!LOW_POWER_MODE)
		return 1;
	return -1;
}

/************************************************************************/
/* REQUEST_ENTER_COMS_TAKEOVER	                                        */
/* @Purpose: Sends a CAN command to the COMS SSM to request that we		*/
/* enter into COMS TAKEOVER MODE.										*/
/************************************************************************/
static int request_enter_coms_takeover(void)
{
	timeout = 100;
	if(send_can_command(0, 0, FDIR_TASK_ID, COMS_ID, ENTER_COMS_TAKEOVER_COM, DEF_PRIO) < 0)
		enter_SAFE_MODE(CAN_ERROR_WITHIN_FDIR);
	while(!COMS_TAKEOVER_MODE && timeout--)
	{
		taskYIELD();		// Wait <= 100ms for COMS SSM to assert COMS_TAKEOVER_MODE
	}
	if(COMS_TAKEOVER_MODE)
		return 1;
	return -1;
}

/************************************************************************/
/* REQUEST_EXIT_COMS_TAKEOVER	                                        */
/* @Purpose: Sends a CAN command to the COMS SSM to request that we		*/
/* exit COMS TAKEOVER MODE.												*/
/************************************************************************/
static int request_exit_coms_takeover(void)
{
	timeout = 100;
	if(send_can_command(0, 0, FDIR_TASK_ID, COMS_ID, EXIT_COMS_TAKEOVER_COM, DEF_PRIO) < 0)
		enter_SAFE_MODE(CAN_ERROR_WITHIN_FDIR);
	while(COMS_TAKEOVER_MODE && timeout--)
	{
		taskYIELD();		// Wait <= 100ms for COMS SSM to deassert COMS_TAKEOVER_MODE
	}
	if(!COMS_TAKEOVER_MODE)
		return 1;
	return -1;
}

/************************************************************************/
/* REQUEST_PAUSE_OPERATIONS		                                        */
/* @Purpose: Sends a CAN command to the SSM to request that we			*/
/* pause operations.													*/
/* @param: ssmID: ID of the SSM (0|1|2)									*/	
/************************************************************************/
static int request_pause_operations(uint8_t ssmID)
{
	timeout = 100;
	if(send_can_command(0, 0, FDIR_TASK_ID, ssmID, PAUSE_OPERATIONS, DEF_PRIO) < 0)
		enter_SAFE_MODE(CAN_ERROR_WITHIN_FDIR);
	if(!ssmID)
	{
		while(!COMS_PAUSED && timeout--)
		{
			taskYIELD();		// Wait <= 100ms for ssmID to pause operations
		}
		if(COMS_PAUSED)
			return 1;
	}
	if(ssmID == 1)
	{
		while(!EPS_PAUSED && timeout--)
		{
			taskYIELD();		
		}
		if(EPS_PAUSED)
			return 1;
	}
	if(ssmID == 2)
	{
		while(!PAY_PAUSED && timeout--)
		{
			taskYIELD();		
		}
		if(PAY_PAUSED)
			return 1;
	}
	return -1;
}

/************************************************************************/
/* REQUEST_RESUME_OPERATIONS		                                    */
/* @Purpose: Sends a CAN command to the SSM to request that we			*/
/* resume operations.													*/
/* @param: ssmID: ID of the SSM (0|1|2)									*/
/************************************************************************/
static int request_resume_operations(uint8_t ssmID)
{
	timeout = 100;
	if(send_can_command(0, 0, FDIR_TASK_ID, ssmID, RESUME_OPERATIONS, DEF_PRIO) < 0)
		enter_SAFE_MODE(CAN_ERROR_WITHIN_FDIR);
	if(!ssmID)
	{
		while(COMS_PAUSED && timeout--)
		{
			taskYIELD();		// Wait <= 100ms for ssmID to resume operations
		}
		if(!COMS_PAUSED)
			return 1;
	}
	if(ssmID == 1)
	{
		while(EPS_PAUSED && timeout--)
		{
			taskYIELD();		
		}
		if(!EPS_PAUSED)
			return 1;
	}
	if(ssmID == 2)
	{
		while(PAY_PAUSED && timeout--)
		{
			taskYIELD();		
		}
		if(!PAY_PAUSED)
			return 1;
	}
	return -1;
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
/* RESTART_TASK					                                        */
/* @Purpose: Deletes the specified task and then recreates it thereby	*/
/* restarting it.														*/
/* @param: task_id: ex: HK_TASK_ID.										*/
/* @param: task_HANDLE: In main.c, we store the task_HANDLE of each task*/
/* in a global variable that can be accessed here. (housekeeping_HANDLE)*/
/* @return: 1 = success, -1 = failure.									*/
/************************************************************************/
int restart_task(uint8_t task_id, TaskHandle_t task_HANDLE)
{
	// Delete the given task and then recreate it.
	uint8_t taskID = task_id;
	if(!taskID && task_HANDLE)
	{
		if(task_HANDLE == housekeeping_HANDLE)
			taskID = HK_TASK_ID;
		else if(task_HANDLE == time_manage_HANDLE)
			taskID = TIME_TASK_ID;
		else if(task_HANDLE == coms_HANDLE)
			taskID = COMS_TASK_ID;
		else if(task_HANDLE == eps_HANDLE)
			taskID = EPS_TASK_ID;
		else if(task_HANDLE == pay_HANDLE)
			taskID = PAY_TASK_ID;
		else if(task_HANDLE == opr_HANDLE)
			taskID = OBC_PACKET_ROUTER_ID;
		else if(task_HANDLE == scheduling_HANDLE)
			taskID = SCHEDULING_TASK_ID;
		else if(task_HANDLE == wdt_reset_HANDLE)
			taskID = WD_RESET_TASK_ID;
		else if(task_HANDLE == memory_manage_HANDLE)
			taskID = MEMORY_TASK_ID;
		else
			vTaskDelete(task_HANDLE);	// Some bullshit task was running that shouldn't be.
	}
	switch(taskID)
	{
		case HK_TASK_ID:
			housekeep_kill(1);
			housekeeping_HANDLE = housekeep();
		case TIME_TASK_ID:
			time_manage_kill(1);
			time_manage_HANDLE = time_manage();
		case COMS_TASK_ID:
			coms_kill(1);
			coms_HANDLE = coms();
		case EPS_TASK_ID:
			eps_kill(1);
			eps_HANDLE = eps();
		case PAY_TASK_ID:
			payload_kill(1);
			pay_HANDLE = payload();
		case OBC_PACKET_ROUTER_ID:
			opr_kill(1);
			opr_HANDLE = obc_packet_router();
		case SCHEDULING_TASK_ID:
			scheduling_kill(1);
			scheduling_HANDLE = scheduling();
		case WD_RESET_TASK_ID:
			wdt_reset_kill(1);
			wdt_reset_HANDLE = wdt_reset();
		case MEMORY_TASK_ID:
			memory_manage_kill(1);
			memory_manage_HANDLE = memory_manage();
		default:
			enter_SAFE_MODE(ERROR_IN_RESTART_TASK);
			return -1;		// SAFE_MODE ?
	}
	return 1;
}

/************************************************************************/
/* DELETE_TASK					                                        */
/* @Purpose: Deletes the specified task.								*/
/* @param: task_id: ex: HK_TASK_ID.										*/
/* @return: 1 = success, -1 = failure.									*/
/************************************************************************/
int delete_task(uint8_t task_id)
{
	// Delete the given task.
	switch(task_id)
	{
		case HK_TASK_ID:
			housekeep_kill(1);
		case TIME_TASK_ID:
			time_manage_kill(1);
		case COMS_TASK_ID:
			coms_kill(1);
		case EPS_TASK_ID:
			eps_kill(1);
		case PAY_TASK_ID:
			payload_kill(1);
		case OBC_PACKET_ROUTER_ID:
			opr_kill(1);
		case SCHEDULING_TASK_ID:
			scheduling_kill(1);
		case WD_RESET_TASK_ID:
			wdt_reset_kill(1);
		case MEMORY_TASK_ID:
			memory_manage_kill(1);
		default:
			enter_SAFE_MODE(ERROR_IN_DELETE_TASK);
			return -1;		// SAFE_MODE ?
	}
	return 1;	
}

/************************************************************************/
/* RECREATE_FIFO				                                        */
/* @Purpose: Deletes the specified fifo and then recreates				*/
/* @param: task_id: ex: HK_TASK_ID.										*/
/* @param: direction: 1 = TO OPR, 0 = FROM OPR.							*/
/* @Note: This function can only be used on fifos that are used for		*/
/* communication between OPR and other PUS tasks.						*/
/* @return: 1 = success, -1 = failure.									*/
/************************************************************************/
int recreate_fifo(uint8_t task_id, uint8_t direction)
{
	clear_fifo_buffer();
	switch(task_id)
	{
		case HK_TASK_ID:
			if(direction)
				recreate_fifo_h(hk_to_obc_fifo);
			else
				recreate_fifo_h(obc_to_hk_fifo);
		case TIME_TASK_ID:
			if(direction)
				recreate_fifo_h(time_to_obc_fifo);
			else
				recreate_fifo_h(obc_to_time_fifo);
		case SCHEDULING_TASK_ID:
			if(direction)
				recreate_fifo_h(sched_to_obc_fifo);
			else
				recreate_fifo_h(obc_to_sched_fifo);
		case MEMORY_TASK_ID:
			if(direction)
				recreate_fifo_h(mem_to_obc_fifo);
			else
				recreate_fifo_h(obc_to_mem_fifo);
		default:
			enter_SAFE_MODE(ERROR_IN_RS5);
				return -1;		// SAFE_MODE ?
	}
	return 1;
}

int recreate_fifo_h(QueueHandle_t *queue_to_recreate)
{
	UBaseType_t fifo_length, item_size;
	fifo_length = 4;
	item_size = 147;
	uint8_t counter;
	if(!*queue_to_recreate)		// FIFO got deleted somehow.
	{
		*queue_to_recreate = xQueueCreate(fifo_length, item_size);
		return 1;
	}
	// Load the contents of the fifo to be cleared into the fdir_fifo_buffer.
	while((xQueueReceive(*queue_to_recreate, test_array1, (TickType_t)1)) && (counter <= 4))
	{
		counter++;
		if(!xQueueSendToBack(fdir_fifo_buffer, test_array1, (TickType_t)1))
			enter_SAFE_MODE(FIFO_ERROR_WITHIN_FDIR);
	}
	vQueueDelete(*queue_to_recreate);
	*queue_to_recreate = xQueueCreate(fifo_length, item_size);
	while(counter)
	{
		if(!xQueueReceive(fdir_fifo_buffer, test_array1, (TickType_t)1))
			enter_SAFE_MODE(FIFO_ERROR_WITHIN_FDIR);
		if(!xQueueSendToBack(*queue_to_recreate, test_array1, (TickType_t)1))
			enter_SAFE_MODE(FIFO_ERROR_WITHIN_FDIR);
	}
	return 1;
}

/************************************************************************/
/* RESTART_TASK					                                        */
/* @Purpose: Hard-resets the specified SSM.								*/
/* @param: ssm_id: (0|1|2) = (COMS|EPS|PAY)								*/
/* @return: 1 = success, -1 = failure.									*/
/************************************************************************/
int reset_SSM(uint8_t ssm_id)
{
	// Set the pin for the SSM low, then back to high again.
	switch(ssm_id)
	{
		case COMS_ID:
			gpio_set_pin_low(COMS_RST_GPIO);
			delay_ms(5);
			gpio_set_pin_high(COMS_RST_GPIO);
		case EPS_ID:
			gpio_set_pin_low(EPS_RST_GPIO);
			delay_ms(5);
			gpio_set_pin_high(EPS_RST_GPIO);		
		case PAY_ID:
			gpio_set_pin_low(PAY_RST_GPIO);
			delay_ms(5);
			gpio_set_pin_high(PAY_RST_GPIO);		
		default:
			enter_SAFE_MODE(ERROR_IN_RESET_SSM);
			return -1;
	}
	return 1;
}

/************************************************************************/
/* CLEAR_FIFO_BUFFER					                                */
/* @Purpose: Clears the fifo buffer by reading from it.					*/
/* @Note: Makes use of test_array1										*/
/************************************************************************/
void clear_fifo_buffer(void)
{
	xQueueReceive(fdir_fifo_buffer, test_array1, (TickType_t)1);
	xQueueReceive(fdir_fifo_buffer, test_array1, (TickType_t)1);
	xQueueReceive(fdir_fifo_buffer, test_array1, (TickType_t)1);
	xQueueReceive(fdir_fifo_buffer, test_array1, (TickType_t)1);
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
	xQueueSendToBack(fdir_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY
	return;
}

/************************************************************************/
/* ENTER_SAFE_MODE				                                        */
/* @Purpose: Puts the OBC into SAFE_MODE. Here a reduced version of all	*/
/* subsidiary tasks is contained.										*/
/* @param: reason: indicates why the system is entering SAFE_MODE. This */
/* is an error code from error_handling.h								*/
/************************************************************************/
static void enter_SAFE_MODE(uint8_t reason)
{
	minute_count = 0;
	diag_last_report_minutes = CURRENT_MINUTE; //will send the first diagnostics report in (collectioninterval) minutes
	diag_old_minute = CURRENT_MINUTE;
	diag_num_hours = 0;
	SAFE_MODE = 1;
	SMERROR = reason;
	// Let the ground the error that occurred, and that we're entering into SAFE_MODE.
	if(reason)
		send_event_report(4, reason, 0, 0);
	send_event_report(1, SAFE_MODE_ENTERED, 0, 0);
	
	// REQUEST ASSISTANCE IF REASON IS NON-ZERO. 
	
	// Pause subsidiary tasks
	vTaskSuspend(housekeeping_HANDLE);
	vTaskSuspend(time_manage_HANDLE);
	vTaskSuspend(coms_HANDLE);
	vTaskSuspend(eps_HANDLE);
	vTaskSuspend(pay_HANDLE);
	vTaskSuspend(scheduling_HANDLE);
	vTaskSuspend(wdt_reset_HANDLE);
	vTaskSuspend(memory_manage_HANDLE);
	// "Pause" SSM operations
	
	while(SAFE_MODE)
	{
		// Reset the watchdog timer.
		wdt_restart(WDT);
		
		if ((CURRENT_MINUTE + 60*diag_num_hours) - diag_last_report_minutes > diag_time_to_wait) { // Collect diagnostics (William: request_diagnostics(), store_diagnostics() can go here)
			//report diagnostics every (collectioninterval) mins
			
			int x;
			x = request_diagnostics_all();	 /* gets data from subsystems	       */
			if (x < 0)
				send_event_report(3, DIAG_ERROR_IN_FDIR, 0, 0); //log an error
			
			x = store_diagnostics();		 /*	-stores in SPI memory		       */
			if (x < 0)
				send_event_report(2, DIAG_ERROR_IN_FDIR, 0, current_diag_fullf); //log an error
			
			send_diag_as_tm();				 /*  -sends data as telemetry	       */
			if(diag_param_report_requiredf)
				send_diag_param_report();	 /* sends parameter report if required */
				
				
			//update the time of last diagnostics report
			diag_last_report_minutes = CURRENT_MINUTE;
			diag_num_hours = 0;
		}
		else if (diag_old_minute > CURRENT_MINUTE) {
			//count how many hours have gone by (i.e. when CURRENT_MINUTE goes from 59 to 0)
			diag_num_hours++;
		}
		diag_old_minute = CURRENT_MINUTE;
		
		
		// Update the absolute time on the satellite
		time_update();
		// Wait for a command from ground.
		exec_commands();
		// Deal with anymore errors that might have arisen.
		check_error();
	}
	
	// Resume subsidiary tasks
	vTaskResume(housekeeping_HANDLE);
	vTaskResume(time_manage_HANDLE);
	vTaskResume(coms_HANDLE);
	vTaskResume(eps_HANDLE);
	vTaskResume(pay_HANDLE);
	vTaskResume(scheduling_HANDLE);
	vTaskResume(wdt_reset_HANDLE);
	vTaskResume(memory_manage_HANDLE);
	return;
}

/************************************************************************/
/* TIME_UPDATE															*/
/* @Purpose: Does the part of what time_manage does that is relared to	*/
/* updating absolute time and CURRENT_TIME within the SSMs.				*/
/************************************************************************/
static void time_update(void)
{
	uint32_t report_timeout = 60;	// Produce a time report once every 60 minutes.
	if (rtc_triggered_a2())
	{
		rtc_get(&time);
		minute_count++;
		if(minute_count == report_timeout)
		report_time();
		update_absolute_time();
		broadcast_minute();
		rtc_reset_a2();
	}
	return;
}

/************************************************************************/
/* RESTART_TASK					                                        */
/* @Purpose: Clears test_array1,2										*/
/************************************************************************/
static void clear_test_arrays(void)
{
	uint16_t i;
	for (i = 0; i < 256; i++)
	{
		test_array1[(uint8_t)i] = 0;
		test_array2[(uint8_t)i] = 0;
	}
	return;
}

/************************************************************************/
/* ENTER_INTERNAL_MEMORY_FALLBACK					                    */
/* @Purpose: Puts the OBC into INTERNAL_MEMORY_FALLBACK mode in which	*/
/* internal memory is used instead of SPI memory.						*/
/************************************************************************/
void enter_INTERNAL_MEMORY_FALLBACK(void)
{
	INTERNAL_MEMORY_FALLBACK_MODE = 1;
	HK_BASE			=	0x0000;		// HK = 512B: 0x0000 - 0x01FF
	EVENT_BASE		=	0x0200;		// EVENT = 512B: 0x0200 - 0x03FF
	SCHEDULE_BASE	=	0x0400;		// SCHEDULE = 1kB: 0x0400 - 0x07FF
	TM_BASE			=	0x0800;		// TM BUFFER = 1kB: 0x0800 - 0x0BFF
	TC_BASE			=	0x0C00;		// TC BUFFER = 1kB: 
	SCIENCE_BASE	=	0x1000;		// SCIENCE = 4kB: 0x1000 - 0x1FFB
	TIME_BASE		=	0x0FFC;		// TIME = 4B: 0x1FFC - 0x1FFF
	MAX_SCHED_COMMANDS = 63;
	LENGTH_OF_HK	= 512;
	send_event_report(2, INTERNAL_MEMORY_FALLBACK, 0, 0);
	return;
}

/************************************************************************/
/* EXIT_INTERNAL_MEMORY_FALLBACK					                    */
/* @Purpose: Takes the OBC out of INTERNAL_MEMORY_FALLBACK mode			*/
/************************************************************************/
void exit_INTERNAL_MEMORY_FALLBACK(void)
{
	INTERNAL_MEMORY_FALLBACK_MODE = 0;
	HK_BASE			=	0x0C000;	// HK = 8kB: 0x0C000 - 0x0DFFF
	EVENT_BASE		=	0x0E000;	// EVENT = 8kB: 0x0E000 - 0x0FFFF
	SCHEDULE_BASE	=	0x10000;	// SCHEDULE = 8kB: 0x10000 - 0x11FFF
	SCIENCE_BASE	=	0x12000;	// SCIENCE = 8kB: 0x12000 - 0x13FFF
	TIME_BASE		=	0xFFFFC;	// TIME = 4B: 0xFFFFC - 0xFFFFF
	MAX_SCHED_COMMANDS = 511;
	LENGTH_OF_HK	= 8192;
	send_event_report(1, INTERNAL_MEMORY_FALLBACK_EXITED, 0, 0);
	return;
}

// Initializes all variables which are local to FDIR.
static void init_vars(void)
{
	housekeep_fumble_count = 0;
	scheduling_fumble_count = 0;
	time_manage_fumble_count = 0;
	memory_manage_fumble_count = 0;
	wdt_reset_fumble_count = 0;
	eps_fumble_count = 0;
	coms_fumble_count = 0;
	payload_fumble_count = 0;
	opr_fumble_count = 0;
	eps_SSM_fumble_count = 0;
	coms_SSM_fumble_count = 0;
	payload_SSM_fumble_count = 0;
	SPI_CHIP_1_fumble_count = 0;
	SPI_CHIP_2_fumble_count = 0;
	SPI_CHIP_3_fumble_count = 0;
	hk_fifo_to_OPR_fumble_count = 0;
	hk_fifo_from_OPR_fumble_count = 0;
	sched_fifo_to_OPR_fumble_count = 0;
	sched_fifo_from_OPR_fumble_count = 0;
	time_fifo_to_OPR_fumble_count = 0;
	time_fifo_from_OPR_fumble_count = 0;
	memory_fifo_to_OPR_fumble_count = 0;
	memory_fifo_from_OPR_fumble_count = 0;
	wdt_fifo_to_OPR_fumble_count = 0;
	wdt_fifo_from_OPR_fumble_count = 0;
	eps_fifo_to_OPR_fumble_count = 0;
	eps_fifo_from_OPR_fumble_count = 0;
	coms_fifo_to_OPR_fumble_count = 0;
	coms_fifo_from_OPR_fumble_count = 0;
	pay_fifo_to_OPR_fumble_count = 0;
	pay_fifo_from_OPR_fumble_count = 0;
	SMERROR = 0;
	clear_test_arrays();
	
	// initialize diagnostics variables
	new_diag_msg_low = 0;
	new_diag_msg_high = 0;
	current_diag_fullf = 0;
	current_diag_definitionf = 0; //start with default definition
	diag_param_report_requiredf = 0;
	collection_interval0 = 30;
	collection_interval1 = 30;
	
	//clear diagnostics values and set up the default definition
	clear_current_diag();
	clear_current_command();
	setup_default_definition();
	set_definition(DEFAULT);
	clear_alternate_diag_definition();
	set_diag_mem_offset();
	
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
	current_command[144] = FDIR_TASK_ID;			// APID of this task
	current_command[140] = ((uint8_t)packet_id) >> 8;
	current_command[139] = (uint8_t)packet_id;
	current_command[138] = ((uint8_t)psc) >> 8;
	current_command[137] = (uint8_t)psc;
	if(xQueueSendToBack(fdir_to_obc_fifo, current_command, (TickType_t)1) < 0)
		enter_SAFE_MODE(FIFO_ERROR_WITHIN_FDIR);
	return;
}



/* --- Diagnostics functions --- */

/************************************************************************/
/* CLEAR_CURRENT_DIAG													*/
/* @Purpose: clears the arrays current_diag[] and diag_updated[]		*/
/* @Note: Modified version of clear_current_hk() from housekeep.c		*/
/************************************************************************/
static void clear_current_diag(void)
{
	uint8_t i;
	for (i=0; i < DATA_LENGTH; i++)
	{
		current_diag[i] = 0;
		diag_updated[i] = 0;
	}
	return;
}

/************************************************************************/
/* CLEAR_ALTERNATE_HK_DEFINITION										*/
/* @Purpose: clears the array current_hk_definition						*/
/* @Note: Modified from housekeep.c										*/
/************************************************************************/
static void clear_alternate_diag_definition(void)
{
	uint8_t i;
	for(i = 0; i < DATA_LENGTH; i++)
	{
		diag_definition1[i] = 0;
	}
	return;
}

/************************************************************************/
/* REQUEST_DIAGNOSTICS_ALL												*/
/* @Purpose: requests data from the subsystems							*/
/* @Note: Straight copied from housekeep.c								*/
/************************************************************************/
static int request_diagnostics_all(void)
{
	if(request_housekeeping(EPS_ID) > 1)	// Request housekeeping from COMS.
		return -1;
	if(request_housekeeping(COMS_ID) > 1)	// Request housekeeping from EPS.
		return -1;
	if(request_housekeeping(PAY_ID) > 1)	// Request housekeeping from PAY.
		return -1;
	
	// Give the SSMs >100ms to transmit their housekeeping
	delay_ms(100);
	return 1;
}

/************************************************************************/
/* STORE_DIAGNOSTICS													*/
/* @Purpose: verifies diagnostics, then calls store_diag_in_spimem()	*/
/* @Note: Copied and edited from housekeep.c							*/
/************************************************************************/
static int store_diagnostics(void)
{
	uint8_t num_parameters = current_diag_definition[134];
	int x = -1;
	uint8_t parameter_name = 0;
	uint8_t i;
	int attempts = 1;
	int* status = 0; // this might be wrong
	int req_data_result = 0;
	
	if (current_diag_fullf)
	{
		return -1;
	}
	clear_current_diag();
	
	while(read_can_hk(&new_diag_msg_high, &new_diag_msg_low, 1234) == 1)
	{
		parameter_name = (new_diag_msg_high & 0x0000FF00) >> 8;	// Name of the parameter for diagnostics (either sensor or variable).
		
		for(i = 0; i < num_parameters; i+=2)
		{
			if (current_diag_definition[i] == parameter_name)
			{
				current_diag[i] = (uint8_t)(new_diag_msg_low & 0x000000FF);
				current_diag[i] = (uint8_t)((new_diag_msg_low & 0x0000FF00) >> 8);
				
				diag_updated[i] = 1;
				diag_updated[i + 1] = 1;
			}
		}
		delay_ms(1); // Allows for more messages to come in. 
	} 
	
	for(i = 0; i < num_parameters; i+=2)
	{
		if(!diag_updated[i])
		{//failed updates are requested 3 times. If they fail, error is reported
			req_data_result = request_sensor_data(FDIR_TASK_ID,get_ssm_id(current_diag_definition[i]),current_diag_definition[i],status);
			while (attempts < 3 && req_data_result == -1){
				attempts++;
				req_data_result = request_sensor_data(FDIR_TASK_ID,get_ssm_id(current_diag_definition[i]),current_diag_definition[i],status);
			}
			
			if (req_data_result == -1){
				//log malfunctioning sensor
				send_event_report(2, DIAG_SENSOR_ERROR_IN_FDIR, get_ssm_id(current_diag_definition[i]), current_diag_definition[i]); //put ssm id and sensor id as data in log
			}
			else {
				current_diag[i] = (uint8_t)(req_data_result & 0x000000FF);
				current_diag[i + 1] = (uint8_t)((req_data_result & 0x0000FF00) >> 8);
				
				diag_updated[i] = 1;
				diag_updated[i + 1] = 1;
			}
		}
	}
	
	/* actually store in SPI memory */
	x = store_diag_in_spimem();  // event report if x < 0, current full =0
	if (x < 0) 
	{
		send_event_report(2, DIAG_SPIMEM_ERROR_IN_FDIR, 0, 0);
		current_diag_fullf = 0;
	}
	
	return 1;
}

/************************************************************************/
/* SET_DIAG_MEM_OFFSET													*/
/* @Purpose: sets the memory offset for diagnostics's					*/
/*	reading/writing to SPIMEM											*/
/************************************************************************/
static void set_diag_mem_offset(void)
{
	spimem_read(DIAG_BASE, current_diag_mem_offset, 4);	// Get the current diagnostics memory offset.
	offset = (uint32_t)(current_diag_mem_offset[0] << 24);
	offset += (uint32_t)(current_diag_mem_offset[1] << 16);
	offset += (uint32_t)(current_diag_mem_offset[2] << 8);
	offset += (uint32_t)current_diag_mem_offset[3];
	
	if(offset == 0) {
		current_diag_mem_offset[3] = 4;
		spimem_write(DIAG_BASE + 3, current_diag_mem_offset + 3, 1);
	}
	return;
}

/************************************************************************/
/* STORE_DIAG_IN_SPIMEM													*/
/* @Purpose: stores the diagnostics which was just collected in spimem	*/
/* along with a timestamp.												*/
/* @Note: When diag memory fills up, this function simply wraps back	*/
/* around to the beginning of diagnostics in memory.					*/
/************************************************************************/
static int store_diag_in_spimem(void)
{
	int x;
	offset = (uint32_t)(current_diag_mem_offset[0] << 24);
	offset += (uint32_t)(current_diag_mem_offset[1] << 16);
	offset += (uint32_t)(current_diag_mem_offset[2] << 8);
	offset += (uint32_t)current_diag_mem_offset[3];
	current_diag_fullf = 0;
	
	x = spimem_write((DIAG_BASE + offset), absolute_time_arr, 4);		// Write the timestamp and then the housekeeping
	if (x < 0) return -1; 
	
	x = spimem_write((DIAG_BASE + offset + 4), &current_diag_definitionf, 1);	// Writes the sID to memory.
	if (x < 0) return -1; 
	
	x = spimem_write((DIAG_BASE + offset + 5), current_diag, 128);
	if (x < 0) return -1; 
	
	offset = (offset + 137) % 16384;								// Make sure diagnostics doesn't overflow into the next section (we gave 16kB to diagnostics = 16384 bits)
	if(offset < 4) offset = 4;
	
	current_diag_mem_offset[3] = (uint8_t)offset;
	current_diag_mem_offset[2] = (uint8_t)((offset & 0x0000FF00) >> 8);
	current_diag_mem_offset[1] = (uint8_t)((offset & 0x00FF0000) >> 16);
	current_diag_mem_offset[0] = (uint8_t)((offset & 0xFF000000) >> 24);
	x = spimem_write(DIAG_BASE, current_diag_mem_offset, 4);			
	return x;
}

/************************************************************************/
/* SETUP_DEFAULT_DEFINITION												*/
/* @Purpose: This function will set the values stored in the default	*/
/* diagnostics definition and will also set it as the current definition*/
/************************************************************************/
static void setup_default_definition(void)
{
	uint8_t i;
	
	for(i = 0; i < DATA_LENGTH; i++)
	{
		diag_definition0[i]=0;
	}
	
	//just copied from housekeep.c
	//might want to change it later
	diag_definition0[136] = 0;							// sID = 0
	diag_definition0[135] = collection_interval0;			// Collection interval = 30 min
	diag_definition0[134] = 36;							// Number of parameters (2B each)
	diag_definition0[81] = PANELX_V;
	diag_definition0[80] = PANELX_V;
	diag_definition0[79] = PANELX_I;
	diag_definition0[78] = PANELX_I;
	diag_definition0[77] = PANELY_V;
	diag_definition0[76] = PANELY_V;
	diag_definition0[75] = PANELY_I;
	diag_definition0[74] = PANELY_I;
	diag_definition0[73] = BATTM_V;
	diag_definition0[72] = BATTM_V;
	diag_definition0[71] = BATT_V;
	diag_definition0[70] = BATT_V;
	diag_definition0[69] = BATTIN_I;
	diag_definition0[68] = BATTIN_I;
	diag_definition0[67] = BATTOUT_I;
	diag_definition0[66] = BATTOUT_I;
	diag_definition0[65] = BATT_TEMP;
	diag_definition0[64] = BATT_TEMP;	//
	diag_definition0[63] = EPS_TEMP;
	diag_definition0[62] = EPS_TEMP;	//
	diag_definition0[61] = COMS_V;
	diag_definition0[60] = COMS_V;
	diag_definition0[59] = COMS_I;
	diag_definition0[58] = COMS_I;
	diag_definition0[57] = PAY_V;
	diag_definition0[56] = PAY_V;
	diag_definition0[55] = PAY_I;
	diag_definition0[54] = PAY_I;
	diag_definition0[53] = OBC_V;
	diag_definition0[52] = OBC_V;
	diag_definition0[51] = OBC_I;
	diag_definition0[50] = OBC_I;
	diag_definition0[49] = SHUNT_DPOT;
	diag_definition0[48] = SHUNT_DPOT;
	diag_definition0[47] = COMS_TEMP;	//
	diag_definition0[46] = COMS_TEMP;
	diag_definition0[45] = OBC_TEMP;	//
	diag_definition0[44] = OBC_TEMP;
	diag_definition0[43] = PAY_TEMP0;
	diag_definition0[42] = PAY_TEMP0;
	diag_definition0[41] = PAY_TEMP1;
	diag_definition0[40] = PAY_TEMP1;
	diag_definition0[39] = PAY_TEMP2;
	diag_definition0[38] = PAY_TEMP2;
	diag_definition0[37] = PAY_TEMP3;
	diag_definition0[36] = PAY_TEMP3;
	diag_definition0[35] = PAY_TEMP4;
	diag_definition0[34] = PAY_TEMP4;
	diag_definition0[33] = PAY_HUM;
	diag_definition0[32] = PAY_HUM;
	diag_definition0[31] = PAY_PRESS;
	diag_definition0[30] = PAY_PRESS;
	diag_definition0[29] = PAY_ACCEL_X;
	diag_definition0[28] = PAY_ACCEL_X;
	diag_definition0[27] = MPPTX;
	diag_definition0[26] = MPPTX;
	diag_definition0[25] = MPPTY;
	diag_definition0[24] = MPPTY;
	diag_definition0[23] = COMS_MODE;	//
	diag_definition0[22] = COMS_MODE;
	diag_definition0[21] = EPS_MODE;	//
	diag_definition0[20] = EPS_MODE;
	diag_definition0[19] = PAY_MODE;
	diag_definition0[18] = PAY_MODE;
	diag_definition0[17] = OBC_MODE;
	diag_definition0[16] = OBC_MODE;
	diag_definition0[15] = PAY_STATE;
	diag_definition0[14] = PAY_STATE;
	diag_definition0[13] = ABS_TIME_D;
	diag_definition0[12] = ABS_TIME_D;
	diag_definition0[11] = ABS_TIME_H;
	diag_definition0[10] = ABS_TIME_H;
	diag_definition0[9] = ABS_TIME_M;
	diag_definition0[8] = ABS_TIME_M;
	diag_definition0[7] = ABS_TIME_S;
	diag_definition0[6] = ABS_TIME_S;
	diag_definition0[5] = SPI_CHIP_1;
	diag_definition0[4] = SPI_CHIP_1;
	diag_definition0[3] = SPI_CHIP_2;
	diag_definition0[2] = SPI_CHIP_2;
	diag_definition0[1] = SPI_CHIP_3;
	diag_definition0[0] = SPI_CHIP_3;
	return;
}

/************************************************************************/
/* SENT_DEFINITION														*/
/* @Purpose: This function will change which definition is being used	*/
/* for creating diagnostics reports.									*/
/* @param: sID: 0 == default, 1 = alternate.							*/
/************************************************************************/
static void set_definition(uint8_t sID)
{
	uint8_t i;
	if(!sID)	// DEFAULT (use diag_definition0)
	{
		for(i = 0; i < DATA_LENGTH; i++)
		{
			current_diag_definition[i] = diag_definition0[i];
		}
		current_diag_definitionf = 0;
		diag_time_to_wait = collection_interval0 * 1000 * 60;
	}
	if(sID == 1) // ALERNATE (use diag_definition1)
	{
		for(i = 0; i < DATA_LENGTH; i++)
		{
			current_diag_definition[i] = diag_definition1[i];
		}
		current_diag_definitionf = 1;
		diag_time_to_wait = collection_interval1 * 1000 * 60;
	}
	return;
}

/************************************************************************/
/* SEND_DIAG_AS_TM														*/
/* @Purpose: This function will downlink all diagnostics to ground		*/
/* by sending 128B chunks to the OBC_PACKET_ROUTER, which in turn will	*/
/* attempt to downlink the telemetry to ground.							*/
/************************************************************************/
static void send_diag_as_tm(void)
{
	uint8_t i;
	clear_current_command();
	//clear previous data
	
	current_command[146] = DIAG_REPORT;
	
	for(i = 0; i < DATA_LENGTH; i++)
	{
		current_command[i] = current_diag[i];
	}
	
	if (xQueueSendToBack(fdir_to_obc_fifo, current_command, (TickType_t)1) != pdTRUE) {
		send_event_report(3, DIAG_ERROR_IN_FDIR, 0, current_command[146]); //log an error
	}
	
	return;
}


/************************************************************************/
/* SEND_DIAG_PARAM_REPORT												*/
/* @Purpose: This function will downlink the current diagnostics		*/
/* definition being used to produce diagnostics reports.				*/
/* Does this by sending current_diag_definition[] to OBC_PACKET_ROUTER.	*/
/************************************************************************/
static void send_diag_param_report(void)
{
	uint8_t i;
	clear_current_command();
	
	current_command[146] = DIAG_DEFINITION_REPORT;
	for(i = 0; i < DATA_LENGTH; i++)
	{
		current_command[i] = current_diag_definition[i];
	}
	
	if (xQueueSendToBack(fdir_to_obc_fifo, current_command, (TickType_t)1) != pdTRUE) {
		send_event_report(3, DIAG_ERROR_IN_FDIR, 0, current_command[146]); //log an error
	}
}
