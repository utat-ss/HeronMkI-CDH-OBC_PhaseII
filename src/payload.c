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

#include "error_handling.h"
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

static void readTemp(void);
static void readEnv(void);
static void readHum(void);
static void readPres(void);
static void readAccel(void);
static void readFL(void);
static void readMIC(void);
static void activate_heater(uint32_t tempval, int sensor_index);
static void setUpSens(void);
static int store_science(uint8_t type, uint8_t* data);
static int pay_spimem_write(uint32_t addr, uint8_t* data_buff, uint32_t size);

/*-----------------------------------------------------------*/

/* Global Variables Prototypes								*/
static uint8_t temp_timebetween, hum_timebetween, pres_timebetween, accel_timebetween, MIC_timebetween, FL_timebetween;
static uint8_t last_temp_time, last_hum_time, last_pres_time, last_accel_time, last_MIC_time, last_FL_time;
static uint8_t count;
static uint32_t epstemp, comsv, comsi;
static uint32_t payv, payi, obcv, obci;
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
		
		readTemp(); // read temperature sensors, decide what to do to heaters
		
		if(count <= 0){//once per minute, read env sensors, save data
			readEnv();
			count = 2;
		}
		
		
		//need a way to decide when we're starting
		// if(timetostart){
		//	if(valvesclosed){
		//		activate_valves(); //start fluid mixing
		//	}
		
				if(CURRENT_MINUTE - last_FL_time >= FL_timebetween){
					readFL();
					last_FL_time = CURRENT_MINUTE;
				}
				
				if(CURRENT_MINUTE - last_MIC_time > MIC_timebetween){
					readMIC();
					last_MIC_time = CURRENT_MINUTE;
				}
		
		// }
		
		
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
	temp_timebetween = 0x1F4; //0.5 min
	hum_timebetween = 0x3E8; //1 min
	pres_timebetween = 0x3E8; 
	accel_timebetween = 0x3E8;
	MIC_timebetween = 0x1E; //30 min
	FL_timebetween = 0x1E; 
	last_temp_time = 0x0;
	last_hum_time = 0x0;
	last_pres_time = 0x0;
	last_accel_time = 0x0;
	last_MIC_time = 0x0;
	last_FL_time = 0x0;
	count = 0;
}


static void readTemp(){
	uint32_t tempval = 0;
	uint8_t temp_sensor = PAY_TEMP0;
	
	for(int i = 0; i < 5; i++){
		temp_sensor += i;
		tempval = request_sensor_data(PAY_TASK_ID, PAY_ID, temp_sensor, 0);
		//save somehow
		activate_heater(tempval, i);
	}
	
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
	readHum();
	readPres();
	readAccel();
}

static void readHum(void){
	uint32_t humval = 0;
	humval = request_sensor_data(PAY_TASK_ID, PAY_ID, PAY_HUM, 0); //status???
	//spimem_read(uint32_t addr, uint8_t* read_buff, uint32_t size)
	//spimem_write(uint32_t addr, uint8_t* data_buff, uint32_t size) //super confused how to use these
}


static void readPres(void){
	uint32_t presval = 0;
	presval = request_sensor_data(PAY_TASK_ID, PAY_ID, PAY_PRESS, 0); //status???
	//spimem_read(uint32_t addr, uint8_t* read_buff, uint32_t size)
	//spimem_write(uint32_t addr, uint8_t* data_buff, uint32_t size) //super confused how to use these
}

static void readAccel(void){
	uint32_t accelval = 0;
	accelval = request_sensor_data(PAY_TASK_ID, PAY_ID, PAY_ACCEL, 0); //status???
	//spimem_read(uint32_t addr, uint8_t* read_buff, uint32_t size)
	//spimem_write(uint32_t addr, uint8_t* data_buff, uint32_t size) //super confused how to use these
}

static void readFL(void){
	uint32_t FLval = 0;
	uint32_t ODval = 0;
	uint8_t OD_PD = PAY_FL_OD_PD0;
	uint8_t FL_PD = PAY_FL_PD0;
	
	
	for(int i = 0; i < 12; i++){
		FL_PD += i;
		OD_PD += i;
		FLval = request_sensor_data(PAY_TASK_ID, PAY_ID, FL_PD, 0);
		ODval = request_sensor_data(PAY_TASK_ID, PAY_ID, OD_PD, 0);
		//figure out how to save
	}
}

static void readMIC(void){
	uint32_t ODval = 0;
	uint8_t OD_PD = PAY_MIC_OD_PD0;	
	
	for(int i = 0; i < 48; i++){
		OD_PD += i;
		ODval = request_sensor_data(PAY_TASK_ID, PAY_ID, OD_PD, 0);
		//figure out how to save
	}
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

int store_science(uint8_t type, uint8_t* data)
{
	uint32_t offset;
	uint8_t size;
	uint8_t* temp_ptr;
	*temp_ptr = type;
	if (spimem_read(SCIENCE_BASE, &offset, 4) < 0)								// FAILURE_RECOVERY needed for each SPI operation.
		return -1;
	spimem_write(SCIENCE_BASE + offset, &temp_ptr, 1);					// Data Type
	spimem_write(SCIENCE_BASE + offset + 1, absolute_time_arr, 4);		// Time Stamp
	if(!type)			// Temperature collection
		size = 10;
	if(type == 1)		// Environmental Sensor Collection
		size = 12;
	if(type == 2)		// Photosensor collection
		size = 144;
	spimem_write(SCIENCE_BASE + offset + 5, data, size);
	offset += (5 + size);
	spimem_write(SCIENCE_BASE, offset, 4);
	return size;
}

//Wrapper function for spimem_write within this file.
//If the function fails after 3 attempts, sends data_buff to error_report
int pay_spimem_write(uint32_t addr, uint8_t* data_buff, uint32_t size)
{
	int attempts = 1;
	//spimem_success is >0 if successful
	uint8_t spimem_success = spimem_write(addr, data_buff, size);
	while (attempts<3 && spimem_success<0){
		spimem_success = spimem_write(addr, data_buff, size);
		attempts++;
	}
	if (spimem_success<0) {
		errorASSERT(PAY_TASK_ID,spimem_success,PAY_SPIMEM_RW_ERROR, data_buff, 0);
		return -1;
		//spimem_write can return -1,-2,-3,-4 in case of an error - does FDIR treat all cases the same?
	}
	else {return 0;}
}
