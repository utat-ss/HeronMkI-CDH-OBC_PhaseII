/*
Author: Keenan Burnett
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

/* Priorities at which the tasks are created. */
#define FDIR_PRIORITY		( tskIDLE_PRIORITY + 5 )

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define FDIR_PARAMETER			( 0xABCD )

/* Functions Prototypes. */
static void prvFDIRTask( void *pvParameters );
TaskHandle_t fdir(void);
static void check_error(void);
static void decode_error(uint32_t error, uint8_t severity, uint8_t task, uint8_t code);
static void check_commands(void);
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

/* Prototypes for resolution sequences */
static void resolution_sequence1(uint8_t code);
static void resolution_sequence1_1(void);
static void resolution_sequence1_2(void);
static void resolution_sequence1_3(void);
static void resolution_sequence1_4(void);
static void resolution_sequence4(uint8_t task, uint8_t command);
static void resolution_sequence5(uint8_t task, uint8_t code);

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
void update_absolute_time(void);

/* External functions used for diagnostics */
extern uint8_t get_ssm_id(uint8_t sensor_name);

/* Local Varibles for FDIR */
static uint8_t current_command[DATA_LENGTH + 10];	// Used to store commands which are sent from the OBC_PACKET_ROUTER.
static uint8_t test_array1[256], test_array2[256];
static uint32_t minute_count;

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
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 10;	// Number entered here corresponds to the number of ticks we should wait.

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

// This function shall be used to check if there is any required action for the FDIR task to deal with.
static void check_error(void)
{
	uint32_t error = 0;
	uint8_t code = 0, task = 0;
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

static void decode_error(uint32_t error, uint8_t severity, uint8_t task, uint8_t code)
{
	// This is where the resolution sequences are going to go.
	TaskHandle_t temp_task = 0;
	eTaskState task_state = 0;
	uint8_t i;
	int x;
	if(severity == 1)
	{
		switch(error)
		{
			case 1:
				if(task != SCHEDULING_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1(code);
			case 2:
				if(task != SCHEDULING_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1(code);
			case 3:
				if(task != SCHEDULING_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1(code);
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
				//resolution_sequence7(code);
			case 8:
				if(task != HK_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1(code);
			case 0x1C:
				if(task != HK_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence1(code);
			case 9:
				if(task != TIME_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				resolution_sequence5(task, code);
			case 10:
				// INTERNAL_MEMORY_FALLBACK_MODE
				enter_SAFE_MODE(SPIMEM_ERROR_DURING_INIT);
			case 11:
									
		}
		
	}
}

static void resolution_sequence1(uint8_t code)
{
	if(code == 0xFF)				// All SPI Memory chips are dead.
		resolution_sequence1_1();
	if(code == 0xFE)				// Usage error on the part of the task that called the function.
		resolution_sequence1_2();
	if(code == 0xFD)				// Spi0_Mutex was not free when this task was called.
		resolution_sequence1_3();
	if(code == 0xFC)
		resolution_sequence1_4();
	return;
}

static void resolution_sequence1_1(void)
{					
	// INTERNAL MEMORY FALLBACK MODE
	send_event_report(2, INTERNAL_MEMORY_FALLBACK, 0, 0);
	return;
}

static void resolution_sequence1_2(void)
{					
	scheduling_fumble_count++;
	if(scheduling_fumble_count == 10)
	{
		restart_task(SCHEDULING_TASK_ID, (TaskHandle_t)0);
		return;
	}
	if(scheduling_fumble_count > 10)
	{
		enter_SAFE_MODE(SCHEDULING_MALFUNCTION);
	}
	return;
}

static void resolution_sequence1_3(void)
{
	TaskHandle_t temp_task = 0;
	eTaskState task_state = 0;
	uint8_t i;
	delay_ms(50);				// Give the currently running operation time to finish.
	if (xSemaphoreTake(Spi0_Mutex, (TickType_t)5000) == pdTRUE)	// Wait for a maximum of 5 seconds.
	{
		sched_fdir_signal = 0;
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
	exit_atomic();
	for (i = 0; i < 50; i++)
	{
		taskYIELD();				// Give the new task ample time to start runnig again.
	}
	// Try to acquire the mutex again.
	if (xSemaphoreTake(Spi0_Mutex, (TickType_t)5000) == pdTRUE)	// Wait for a maximum of 5 seconds.
	{
		sched_fdir_signal = 0;
		xSemaphoreGive(Spi0_Mutex);		// Nothing seems to be wrong.
		return;
	}
	// Something has gone wrong, etner SAFE_MODE and let ground know.
	enter_SAFE_MODE(SPI0_MUTEX_MALFUNCTION);
	return;
}

static void resolution_sequence1_4(void)
{					
	uint8_t i;
	int x;
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
		// INTERNAL MEMORY FALLBACK MODE
		sched_fdir_signal = 0;	// Issue resolved.
		return;
	}
	// At least one of the chips is working or has not fumbled too many times.
	clear_test_arrays();
	for(i = 0; i < 256; i++)
	{
		test_array1[i] = i;
	}
	
	// Check to see if reading/writing is possible from the chip which malfunctioned.
	if(SPI_HEALTH1)
	{
		if( spimem_write(0x00, test_array1, 256) < 0)
		{
			// INTERNAL MEMORY FALLBACK MODE.
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		x = spimem_read(0x00, test_array2, 256);
		{
			// INTERNAL MEMORY FALLBACK MODE.
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		for(i = 0; i < 256; i++)
		{
			if(test_array2[i] != test_array1[i])
			SPI_HEALTH1 = 0;
		}
		if(SPI_HEALTH1)
		{
			sched_fdir_signal = 0;	// The issue was only temporary.
			return;
		}
	}
	else if(SPI_HEALTH2)			// Try switching to a different chip
	{
		if( spimem_write(0x00, test_array1, 256) < 0)
		{
			// INTERNAL MEMORY FALLBACK MODE.
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		x = spimem_read(0x00, test_array2, 256);
		{
			// INTERNAL MEMORY FALLBACK MODE.
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		for(i = 0; i < 256; i++)
		{
			if(test_array2[i] != test_array1[i])
			SPI_HEALTH2 = 0;	// Try with a different chip.
		}
		if(SPI_HEALTH2)
		{
			sched_fdir_signal = 0;	// The issue was only temporary.
			return;
		}
	}
	else if(SPI_HEALTH3)
	{
		if( spimem_write(0x00, test_array1, 256) < 0)
		{
			// INTERNAL MEMORY FALLBACK MODE.
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		x = spimem_read(0x00, test_array2, 256);
		{
			// INTERNAL MEMORY FALLBACK MODE.
			enter_SAFE_MODE(SPI_FAILED_IN_FDIR);
		}
		for(i = 0; i < 256; i++)
		{
			if(test_array2[i] != test_array1[i])
			SPI_HEALTH3 = 0;	// Try with a different chip.
		}
		if(SPI_HEALTH3)
		{
			sched_fdir_signal = 0;	// The issue was only temporary.
			return;
		}
	}
	// There are no functional chips, INTERNAL MEMORY FALLBACK MODE
	sched_fdir_signal = 0;
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

static void resolution_sequence7(uint8_t parameter)
{
	uint8_t ssmID = 0xFF;
	uint32_t data = 0;
	ssmID = get_ssm_id(parameter);
	if(ssmID == OBC_ID)
	{
		enter_SAFE_MODE(OBC_PARAM_FAILED);
		return;
	}
	if(ssmID == COMS_ID)
	{
		data = request_sensor_data()
	}
}


static void check_commands(void)
{
	// Check for command from the OPR.
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


// A return of 1 indicates that the action was completed successfully.
// A return of -1 of course means that this function failed.
int recreate_fifo(uint8_t task_id, uint8_t direction)
{
	clear_fifo_buffer();
	switch(task)
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

// *Makes use of test_array1
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
	current_command[3] = severity;
	current_command[2] = report_id;
	current_command[1] = param1;
	current_command[0] = param0;
	xQueueSendToBack(fdir_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY
	return;
}

static void enter_SAFE_MODE(uint8_t reason)
{
	minute_count = 0;
	SAFE_MODE = 1;
	SMERROR = reason;
	// Let the ground the error that occurred, and that we're entering into SAFE_MODE.
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
		// Collect diagnostics (William: request_diagnostics(), store_diagnostics() can go here)
		
		// Update the absolute time on the satellite
		time_update();
		// Wait for a command from ground.
		exec_commands();
		// Deal with anymore errors that might have arisen.
		check_error();
	}
	return;
}

// Does the part of what time_manage does that is related to updating absolute time and CURRENT_MINUTE within the SSMs.
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

int reprogram_SSM(uint8_t ssm_id)
{
	// Code for reprogramming the SSM can go here.
}

static void clear_test_arrays(void)
{
	uint8_t i;
	for (i = 0; i < 256; i++)
	{
		test_array1[i] = 0;
		test_array2[i] = 0;
	}
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
	return;
}
