/*
Author: Keenan Burnett, Karen Morenz
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

#define PAY_LOOP_TIMEOUT		10000							// Specifies how many ticks to wait before running payload again.

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

static uint16_t read_temp_h(int sensorNumber, int* s);
static uint8_t read_temps(int* s);
static void read_env(int* s);
static uint16_t read_hum(int* s);
static uint16_t read_pres(int* s);
static uint16_t read_accel(int* s);
static void read_opts(int* s);
static void manage_heaters(void);
static void set_up_sens(void);
static int store_science(uint8_t type, uint8_t* data);
/*-----------------------------------------------------------*/

/* Global Variables Prototypes								*/
static uint8_t opts_timebetween;
static uint8_t last_Optstime;
static uint8_t count;
static uint8_t valvesclosed;
static int* status;
static uint8_t temp_sensor;
static uint8_t env[16];
static uint16_t tempval[5];
static uint16_t humval;
static uint16_t presval;
static uint16_t accelval;
static uint16_t optval[144];//FL experiment first, FL reading then OD reading for each well, and then MIC OD after
static uint16_t temp;
static uint8_t OD_PD, FL_PD;
static uint32_t offset;
static uint8_t size;
static uint8_t* temp_ptr;
static TickType_t xLastWakeTime;
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
		&temp_HANDLE );

	return temp_HANDLE;
}

/************************************************************************/
/* PAYLOAD TASK */
/* This task encapsulates the high-level software functionality of the	*/
/* payload subsystem on this satellite.									*/
/*																		*/
/************************************************************************/
static void prvPayloadTask(void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == PAYLOAD_PARAMETER );
	TickType_t last_tick_count = xTaskGetTickCount();
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
	*status = 0;
	/* Declare Variables Here */
	
	set_up_sens();
	/* @non-terminating@ */	
	for( ;; )
	{
		if(xTaskGetTickCount() - last_tick_count > PAY_LOOP_TIMEOUT)
		{
			read_temps(status);
			if(*status > 0)
				manage_heaters();
		
			if(count > 6)	//once per minute, read env sensors, save data (count every 500 ms)
			{
				read_env(status);
				count = 0;
			}
		
			if(experiment_started)
			{
				if(valvesclosed)
				{
					send_can_command(0, 0, PAY_TASK_ID, PAY_ID, OPEN_VALVES, DEF_PRIO);	//(see data_collect.c)
					valvesclosed = 0;
				}
				if(CURRENT_MINUTE - last_Optstime >= opts_timebetween)
				{
					send_can_command(0, 0, PAY_TASK_ID, PAY_ID, COLLECT_PD, DEF_PRIO);
					last_Optstime = CURRENT_MINUTE;
				}
				if(pd_collectedf)
				{
					read_opts(status);
				}	
			}
			count++;
			last_tick_count = xTaskGetTickCount();	
		}
	}
}

// Declare Static Functions here.

/************************************************************************/
/* SETUPSENS				                                            */
/* @Purpose: This function initializes all of the global variables used */
/* in running the sensors												*/
/************************************************************************/
static void set_up_sens(void)
{
	//time between in ms in hexadecimal
	opts_timebetween = 0x1E; 
	last_Optstime = 0x0;
	count = 0;
	valvesclosed = 1;
}

/************************************************************************/
/* READTEMP					                                            */
/* @Purpose: Requests temperature data from the payload SSM.			*/
/* @param: sensorNumber: temp_sensor = PAY_TEMP0 + sensorNumber			*/
/* @param: *s: 1 = success, -1 = failure.								*/
/* @return: The value of the temperature sensor which was read.			*/
/************************************************************************/
static uint16_t read_temp_h(int sensorNumber, int* s)
{
	*status = 0;
	temp_sensor = PAY_TEMP0 + sensorNumber ;
	tempval[temp_sensor] = request_sensor_data(PAY_TASK_ID, PAY_ID, temp_sensor, status);
	*s = *status;
	if(*status < 0)
		return 0;

	return tempval[temp_sensor];
}

static uint8_t read_temps(int* s)
{
	for(uint8_t i = 0; i < 5; i++)
	{
		read_temp_h(i, status);
		*s = *status;
		if(*status < 0)
			return 0;
	}
	return 1;
}

/************************************************************************/
/* MANAGE_HEATERS			                                            */
/* @Purpose: Activates the heaters in the payload depending on what		*/
/* the temperature is.													*/
/************************************************************************/
static void manage_heaters(void)
{
	uint8_t i;
	for(i = 0; i < 5; i++)
	{
		if(tempval[i] > TARGET_TEMP){
			//heater off (acc. sensor_index)
			if(tempval[i] > TARGET_TEMP + TEMP_RANGE){
				// freak out? Send notification???
			}
		}
		else if(tempval[i] < TARGET_TEMP){
			//heater on (acc. sensor_index)
			if(tempval[i] < TARGET_TEMP - TEMP_RANGE){
				// turn the heaters on EXTRA. freak out and send notification???
			}
		}	
	}
	return;
}

/************************************************************************/
/* READENV					                                            */
/* @Purpose: Reads the environmental sensors of the payload by sending	*/
/* and receiving CAN messages.											*/
/* @Note: The data is stored in SPI memory after retrieval is complete.	*/
/************************************************************************/
static void read_env(int* s)
{
	uint16_t sens_val;
	sens_val = read_hum(status);
	env[0] = (uint8_t)sens_val;
	env[1] = (uint8_t)(sens_val >> 8);
	*s = *status;
	if(*status < 0)
		return;
	sens_val = read_pres(status);
	env[2] = (uint8_t)sens_val;
	env[3] = (uint8_t)(sens_val >> 8);
	*s = *status;
	if(*status < 0)
		return;
	sens_val = read_accel(status);
	env[4] = (uint8_t)sens_val;
	env[5] = (uint8_t)(sens_val >> 8);
	*s = *status;
	if(*status < 0)
		return;
	read_temps(status);
	*s = *status;
	if(*status < 0)
		return;
	for(int i = 0; i < 5; i++)
	{
		env[2 * i + 6] = (uint8_t)tempval[i];
		env[2 * i + 7] = (uint8_t)(tempval[i] >> 8);		
	}
	store_science(0, env);
}

/************************************************************************/
/* READHUM					                                            */
/* @Purpose: Requests humidity data from the payload SSM.				*/
/* @return: The value of the humidity sensor which was read.			*/
/************************************************************************/
static uint16_t read_hum(int* s)
{
	humval = 0;
	*status = 0;
	humval = (uint16_t)request_sensor_data(PAY_TASK_ID, PAY_ID, PAY_HUM, status);
	*s = *status;
	if(*status < 0)
		return 0;
	return humval;
}

/************************************************************************/
/* READPRESS					                                        */
/* @Purpose: Requests pressure data from the payload SSM.				*/
/* @return: The value of the pressure sensor which was read.			*/
/************************************************************************/
static uint16_t read_pres(int* s)
{
	presval = 0;
	*status = 0;
	presval = (uint16_t)request_sensor_data(PAY_TASK_ID, PAY_ID, PAY_PRESS, status);
	*s = *status;
	if(*status < 0)
		return 0;
	return presval;
}

/************************************************************************/
/* READACCEL				                                            */
/* @Purpose: Requests acceleration data from the payload SSM.			*/
/* @return: The value of the acceleration sensor which was read.		*/
/************************************************************************/
static uint16_t read_accel(int* s)
{
	accelval = 0;
	*status = 0;
	accelval = (uint16_t)request_sensor_data(PAY_TASK_ID, PAY_ID, PAY_ACCEL, status);
	*s = *status;
	if(*status < 0)
		return 0;
	return accelval;
}

/************************************************************************/
/* READOPTS					                                            */
/* @Purpose: Requests optical data from the payload SSM.				*/
/* It also stores the received photodiode values into SPI memory		*/
/************************************************************************/
static void read_opts(int* s)
{
	OD_PD = PAY_FL_OD_PD0;
	FL_PD = PAY_FL_PD0;
	*status = 0;
	for(int i = 0; i < 12; i++)
	{
		FL_PD += i;
		OD_PD += i;
		temp = (uint16_t)request_sensor_data(PAY_TASK_ID, PAY_ID, FL_PD, status);
		*s = *status;
		if(*status < 0)
			return;
		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, (TickType_t)50);			// Sleep task for 50 ticks. 
		optval[4 * i]		= (uint8_t)temp;
		optval[4 * i + 1]	= (uint8_t)(temp >> 8);
		temp = (uint16_t)request_sensor_data(PAY_TASK_ID, PAY_ID, OD_PD, status);
		*s = *status;
		if(*status < 0)
			return;
		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, (TickType_t)50);			// Sleep task for 50 ticks.
		optval[4 * i + 2]	= (uint8_t)temp;
		optval[4 * i + 3]	= (uint8_t)(temp >> 8);
	}

	OD_PD = PAY_MIC_OD_PD0;	
	
	for(int i = 0; i < 48; i++)
	{
		OD_PD += i;
		temp = (uint16_t)request_sensor_data(PAY_TASK_ID, PAY_ID, OD_PD, status);
		*s = *status;
		if(*status < 0)
			return;
		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, (TickType_t)50);			// Sleep task for 50 ticks.
		optval[i * 2 + 48]		= (uint8_t)temp; 
		optval[i * 2 + 48 + 1]	=  (uint8_t)(temp >> 8);
	}
	
	store_science(2, optval);
}

/************************************************************************/
/* STORE_SCIENCE				                                        */
/* @Purpose: Depending on (type), this function stores different amount */
/* of data into SPI memory along with a time stamp from the RTC.		*/
/* @return: The amount of data (bytes) which were written to memory.	*/
/************************************************************************/
static int store_science(uint8_t type, uint8_t* data)
{
	*temp_ptr = type;
	if (spimem_read(SCIENCE_BASE, &offset, 4) < 0)
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

// This function will kill this task.
// If it is being called by this task 0 is passed, otherwise it is probably the FDIR task and 1 should be passed.
void payload_kill(uint8_t killer)
{
	// Kill the task.
	if(killer)
		vTaskDelete(pay_HANDLE);
	else
		vTaskDelete(NULL);
	return;
}
