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
* 05/03/2016	Bill: Adding beacon control.
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
/* Global variable includes */
#include "global_var.h"

/* Priorities at which the tasks are created. */
#define Coms_PRIORITY	( tskIDLE_PRIORITY + 1 ) // Lower the # means lower the priority
/* Values passed to the two tasks just to check the task parameter
functionality. */
#define COMS_PARAMETER	( 0xABCD )

/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
#define COMS_LOOP_TIMEOUT		10000							// Specifies how many ticks to wait before running coms again.

#define COMS_BEACON_WAIT		1000 * 60 * 5					// Send the beacon once every 5 minutes
#define COMS_PASS_LOW			1000 * 60 * 10					// Start sending the beacon 10 minutes after receiving the last tele-command					
#define COMS_PASS_HIGH			1000 * 60 * 70					// Stop sending the beacon 70 minutes after receiving the last tele-command
																// Each pass should be around 90 minutes long
																// Stop sending the beacon before the pass will begin again

/*-----------------------------------------------------------*/

/* Variables												 */
static TickType_t pass_control_timer;

/* Function Prototypes										 */
static void prvComsTask( void *pvParameters );
TaskHandle_t coms(void);
void coms_kill(uint8_t killer);

void update_pass_timer(void);
uint8_t should_send_beacon(void);
uint8_t should_send_tm(void);
/*-----------------------------------------------------------*/

/************************************************************************/
/* COMS (Function) 														*/
/* @Purpose: This function is simply used to create the COMS task below	*/
/* in main.c															*/
/************************************************************************/
TaskHandle_t coms( void )
{
	/* Start the two tasks as described in the comments at the top of this
	file. */
	TaskHandle_t temp_HANDLE = 0;
	xTaskCreate( prvComsTask, /* The function that implements the task. */
		"ON", /* The text name assigned to the task - for debug only as it is not used by the kernel. */
		configMINIMAL_STACK_SIZE, /* The size of the stack to allocate to the task. */
		( void * ) COMS_PARAMETER, /* The parameter passed to the task - just to check the functionality. */
		Coms_PRIORITY, /* The priority assigned to the task. */
		&temp_HANDLE ); /* The task handle is not required, so NULL is passed. */

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached. If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks to be created. See the memory management section on the
	FreeRTOS web site for more details. */
	return temp_HANDLE;
}

/************************************************************************/
/* COMS TASK */
/* This task encapsulates the high-level software functionality of the	*/
/* communication subsystem on this satellite.							*/
/* @Note: The current telemetry packet that needs to be downlinked is	*/
/* placed in current_tm[] and the current and next telecommand that were*/
/* received are placed in current_tc[] and next_tc[] respectively.		*/
/************************************************************************/
static void prvComsTask(void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == COMS_PARAMETER );
	TickType_t last_tick_count = xTaskGetTickCount();
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
	int* status = 0;
	uint32_t data = 0;
	
	/* variables for beacon control */
	TickType_t last_beacon;
	pass_control_timer = xTaskGetTickCount(); last_beacon = xTaskGetTickCount();
	uint8_t enable_coms_rx = 0;
	
	/* @non-terminating@ */	
	for( ;; )
	{
		if(xTaskGetTickCount() - last_tick_count > COMS_LOOP_TIMEOUT)
		{
			data = request_sensor_data(COMS_TASK_ID, COMS_ID, COMS_TEMP, status);
			last_tick_count = xTaskGetTickCount();	
		}	
		
		
		/* beacon control */
		
		if (should_send_beacon()) /* check if we are OK to send the beacon */
		{ /* stop sending beacon when we are approaching the pass (b/c we will be communicating with ground station then) */
			enable_coms_rx = 1;
			
			if (xTaskGetTickCount() - last_beacon > COMS_BEACON_WAIT) /* send beacon every 5 minutes */
			{
				
				
				//get housekeeping data to send
				
				//attempt to send some hk data to COMS SSM (via packet router) for the beacon
				if (get_beacon_data() >= 0)
				{
					//update beacon timer
					last_beacon = xTaskGetTickCount();
				}
								
			}
		}
		else if (enable_coms_rx) 
		{ /* after a certain amount of time, enable RX on the COMS to listen for telecommands (pass should be happening) */
			
			
			if (xSemaphoreTake(Can0_Mutex, (TickType_t) 0) == pdTRUE)		// Attempt to acquire CAN1 Mutex, block for 1 tick.
			{
				if (send_can_command_h2(0x00, 0x00, COMS_TASK_ID, COMS_ID, RX_ENABLE, COMMAND_PRIO) < 0)
				{
					//failed to send
				}
				else {
					//clear the flag (only need to send once)
					enable_coms_rx = 0;
				}
				xSemaphoreGive(Can0_Mutex);
									
			}
		}
		
		
	}
}

// Static helper functions may be defined below.

// This function will kill this task.
// If it is being called by this task 0 is passed, otherwise it is probably the FDIR task and 1 should be passed.
void coms_kill(uint8_t killer)
{
	// Kill the task.
	if(killer)
		vTaskDelete(coms_HANDLE);
	else
		vTaskDelete(NULL);
	return;
}

void update_pass_timer(void) 
{
	/* pass_control_timer will contain the last time a tele-command was received */
	pass_control_timer = xTaskGetTickCount();
}

uint8_t should_send_beacon(void)
{
	/* based on the pass timer, decide when to send the beacon */
	if ( xTaskGetTickCount() - pass_control_timer > COMS_PASS_LOW  &&  xTaskGetTickCount() - pass_control_timer < COMS_PASS_HIGH )
		return 1;
	else
		return 0;
}

uint8_t should_send_tm(void)
{
	/* based on the pass timer, decide when to send TM responses to ground station */
	if (xTaskGetTickCount() - pass_control_timer < COMS_PASS_LOW)
		return 1;
	else
		return 0;
}