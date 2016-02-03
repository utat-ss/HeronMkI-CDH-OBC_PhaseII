/*
Author: Keenan Burnett
***********************************************************************
* FILE NAME: payload.c
*
* PURPOSE:
* This file is to be used for the high-level payload-related software on the satellite.
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
*/

/* Standard includes.										 */
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
#define Payload_PRIORITY	( tskIDLE_PRIORITY + 1 ) // Lower the # means lower the priority
/* Values passed to the two tasks just to check the task parameter
functionality. */
#define PAYLOAD_PARAMETER	( 0xABCD )

/* Relevant Boundaries for payload system		*/
#define MAX_NUM_TRIES	0xA
#define TARGET_TEMP	0x1E //30 deg C
#define TEMP_RANGE 0x2
/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

/* Function Prototypes										 */
static void prvPayloadTask( void *pvParameters );
TaskHandle_t payload(void);
void payload_kill(uint8_t killer);

static uint8_t readTemp(int);
static void readEnv(void);
static uint8_t readHum(void);
static uint8_t readPres(void);
static uint8_t readAccel(void);
static void readOpts(void);
static void activate_heater(uint32_t tempval, int sensor_index);
static void setUpSens(void);
static void store_science(uint8_t type, uint32_t time, uint8_t *data );

/*-----------------------------------------------------------*/

/* Global Variables Prototypes								*/
static uint8_t opts_timebetween;
static uint8_t last_Optstime;
static uint8_t count;
static uint8_t valvesclosed;
static uint8_t start_Experiment; // Dummy variable for now
/*-----------------------------------------------------------*/

/************************************************************************/
/* PAYLOAD (Function) 													*/
/* @Purpose: This function is simply used to create the PAYLOAD task    */
/* in main.c															*/
/************************************************************************/
TaskHandle_t payload( void )
{
	/* Start the two tasks as described in the comments at the top of this
	file. */
	TaskHandle_t temp_HANDLE = 0;
	xTaskCreate( prvPayloadTask, /* The function that implements the task. */
		"ON", /* The text name assigned to the task - for debug only as it is not used by the kernel. */
		configMINIMAL_STACK_SIZE, /* The size of the stack to allocate to the task. */
		( void * ) PAYLOAD_PARAMETER, /* The parameter passed to the task - just to check the functionality. */
		Payload_PRIORITY, /* The priority assigned to the task. */
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
/*																		*/
/************************************************************************/
static void prvPayloadTask(void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == PAYLOAD_PARAMETER );
	TickType_t xLastWakeTime;
	const TickType_t xTimeToWait = 15; // Number entered here corresponds to the number of ticks we should wait.
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/

	/* Declare Variables Here */
	
	setUpSens();

	/* @non-terminating@ */	
	for( ;; )
	{
		// Write your application here.
		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);		// This is what delays your task if you need to yield. Consult CDH before editing.
		
		for(int i = 0; i < 5; i++){
			readTemp(i); // read temperature sensors, decide what to do to heaters
		}
		
		
		if(count <= 0){//once per minute, read env sensors, save data (count every 500 ms)
			readEnv();
			count = 60;
		}
		
		
		if(start_Experiment){
			if(valvesclosed){
				send_can_command(0, 0, PAY_TASK_ID, PAY_ID, OPEN_VALVES, DEF_PRIO) ;//(see data_collect.c)
				valvesclosed = 0;
			}
		
			if(CURRENT_MINUTE - last_Optstime >= opts_timebetween){
				send_can_command(0, 0, PAY_TASK_ID, PAY_ID, COLLECT_PD, DEF_PRIO) ;
				last_Optstime = CURRENT_MINUTE;
			}

			if(pd_collectedf){
				readOpts();
			}	
		
		}
		
		
		vTaskDelayUntil(&xLastWakeTime, 50); //wait 50 ticks = 500 ms
		count -= 1;
	}
}

// Declare Static Functions here.

/****************************************************************************************/
/* SET UP SENS			
									
/* @Purpose: This function initializes all of the global variables used in running
			the sensors									
																						*/
/****************************************************************************************/
static void setUpSens(void)
{
	//time between in ms in hexadecimal
	opts_timebetween = 0x1E; 
	last_Optstime = 0x0;
	count = 0;
	valvesclosed = 1;
}


static uint8_t readTemp(int sensorNumber){
	uint8_t tempval;
	uint8_t temp_sensor;
	
	temp_sensor = PAY_TEMP0 + sensorNumber ;
	tempval= request_sensor_data(PAY_TASK_ID, PAY_ID, temp_sensor, 0);
	activate_heater(tempval, sensorNumber);

	return tempval;
}

static void activate_heater(uint32_t tempval, int sensor_index){
	if(tempval > TARGET_TEMP){
		//heater off (acc. sensor_index)
		if(tempval > TARGET_TEMP + TEMP_RANGE){
			// freak out? Send notification???
		}
	}
	else if(tempval < TARGET_TEMP){
		//heater on (acc. sensor_index)
		if(tempval < TARGET_TEMP - TEMP_RANGE){
			// turn the heaters on EXTRA. freak out and send notification???
		}
	}
}

static void readEnv(){
	uint8_t env[8];
	uint8_t tempval[5];

	env[0] = readHum();
	env[1] = readPres();
	env[2] = readAccel();
	for(int i = 0; i < 5; i++){
		env[i+3] = readTemp(i);
	}

	store_science(0, CURRENT_SECOND, env);
}

static uint8_t readHum(void){
	uint8_t humval = 0;
	humval = request_sensor_data(PAY_TASK_ID, PAY_ID, PAY_HUM, 0); //status???
	return humval;
}


static uint8_t readPres(void){
	uint8_t presval = 0;
	presval = request_sensor_data(PAY_TASK_ID, PAY_ID, PAY_PRESS, 0); 
	return presval;
}

static uint8_t readAccel(void){
	uint8_t accelval = 0;
	accelval = request_sensor_data(PAY_TASK_ID, PAY_ID, PAY_ACCEL, 0); 
	return accelval;
}

static void readOpts(void){
	uint8_t optval[72];//FL experiment first, FL reading then OD reading for each well, and then MIC OD after
	uint8_t OD_PD = PAY_FL_OD_PD0;
	uint8_t FL_PD = PAY_FL_PD0;
	
	for(int i = 0; i < 12; i++){
		FL_PD += i;
		OD_PD += i;
		optval[2*i] = request_sensor_data(PAY_TASK_ID, PAY_ID, FL_PD, 0);
		optval[2*i+1] = request_sensor_data(PAY_TASK_ID, PAY_ID, OD_PD, 0);
	}

	OD_PD = PAY_MIC_OD_PD0;	
	
	for(int i = 0; i < 48; i++){
		OD_PD += i;
		optval[i+24] = request_sensor_data(PAY_TASK_ID, PAY_ID, OD_PD, 0);
		//figure out how to save
	}
	
	store_science(1, CURRENT_SECOND, optval);//what should I be sending for time
}

static void store_science(uint8_t type, uint32_t time, uint8_t *data ){// type = all temp data, all other env data, or all photodiodes) data in an array (uint8_t temperature[10] for example)

}

// This function will kill this task.
// If it is being called by this task 0 is passed, otherwise it is probably the FDIR task and 1 should be passed.
void payload_kill(uint8_t killer)
{
	// Free the memory that this task allocated.
	// Kill the task.
	if(killer)
		vTaskDelete(pay_HANDLE);
	else
		vTaskDelete(NULL);
	return;
}
