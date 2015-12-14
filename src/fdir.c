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
static void send_event_report(uint8_t severity, uint8_t report_id, uint8_t param1, uint8_t param0);
int restart_task(uint8_t task_id);
static void time_update(void);
static void enter_SAFE_MODE(uint8_t reason);

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

/* Local Varibles for FDIR */
static uint8_t current_command[DATA_LENGTH + 10];	// Used to store commands which are sent from the OBC_PACKET_ROUTER.
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
static uint8_t SMERROR;


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
	if(severity == 1)
	{
		switch(error)
		{
			case 1:
				if(task != SCHEDULING_TASK_ID)
					enter_SAFE_MODE(INC_USAGE_OF_DECODE_ERROR);
				if(code == 0xFF)				// All SPI Memory chips are dead.
				{
					// INTERNAL MEMORY FALLBACK MODE
					send_event_report(2, INTERNAL_MEMORY_FALLBACK, 0, 0);
					return;
				}
				if(code == 0xFE)				// Usage error on the part of the task that called the function.
				{
					scheduling_fumble_count++;
					if(scheduling_fumble_count == 10)
					{
						restart_task(SCHEDULING_TASK_ID);
						return;
					}
					if(scheduling_fumble_count > 10)
					{
						enter_SAFE_MODE(SCHEDULING_MALFUNCTION);
					}
				}
				if(code == 0xFD)				// Spi0_Mutex was not free when this task was called.
				{
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
					restart_task(temp_task);
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
				}
				if(code == 0xFC)
				{
					// ....
				}
		
		}
		
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
			enter_SAFE_MODE();
			return -1;		// SAFE_MODE ?
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
			enter_SAFE_MODE();
			return -1;
	}
	return 1;
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
	SMERROR = 0;
	return;
}
