/*
Author: Keenan Burnett
***********************************************************************
* FILE NAME: coms.c
*
* PURPOSE:
* This file is to be used for the high-level power-related software on the satellite.
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
/* CAN Function includes */
#include "can_func.h"
/* Priorities at which the tasks are created. */
#define Eps_PRIORITY	( tskIDLE_PRIORITY + 1 ) // Lower the # means lower the priority
/* Values passed to the two tasks just to check the task parameter
functionality. */
#define EPS_PARAMETER	( 0xABCD )

/* Relevant Boundaries for power system		*/
#define MAX_NUM_TRIES	0xA
#define DUTY_INCREMENT	0x6
/*-----------------------------------------------------------*/

/* Function Prototypes										 */
static void prvEpsTask( void *pvParameters );
void eps(void);
static void getxDirection(void);
static void getyDirection(void);
static uint8_t setxDuty(void);
static uint8_t setyDuty(void);
static uint32_t get_sensor_data(uint8_t sensor_id);
static void set_variable_value(uint8_t variable_name, uint8_t new_var_value);
/*-----------------------------------------------------------*/


/* Global Variables Prototypes								*/
static uint8_t xDirection, yDirection, xDuty, yDuty;
static uint32_t pxp_last, pyp_last;
/*-----------------------------------------------------------*/

/************************************************************************/
/* EPS (Function) 														*/
/* @Purpose: This function is simply used to create the EPS task below	*/
/* in main.c															*/
/************************************************************************/
void eps( void )
{
	/* Start the two tasks as described in the comments at the top of this
	file. */
	xTaskCreate( prvEpsTask, /* The function that implements the task. */
		"ON", /* The text name assigned to the task - for debug only as it is not used by the kernel. */
		configMINIMAL_STACK_SIZE, /* The size of the stack to allocate to the task. */
		( void * ) EPS_PARAMETER, /* The parameter passed to the task - just to check the functionality. */
		Eps_PRIORITY, /* The priority assigned to the task. */
		NULL ); /* The task handle is not required, so NULL is passed. */

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached. If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks to be created. See the memory management section on the
	FreeRTOS web site for more details. */
	return;
}

/************************************************************************/
/* PRVEPSTask															*/
/* @Purpose: This task contains all the high level software required to */
/* run the EPS Subsystem.												*/
/************************************************************************/
static void prvEpsTask(void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == EPS_PARAMETER );
	TickType_t xLastWakeTime;
	const TickType_t xTimeToWait = 15; // Number entered here corresponds to the number of ticks we should wait.
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/

	/* Declare Variables Here */
	uint32_t battmv, battv, batti, battemp;
	uint32_t epstemp, comsv, comsi;
	uint32_t payv, payi, obcv, obci;
	

	/* @non-terminating@ */	
	for( ;; )
	{
		// Write your application here.
		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);		// This is what delays your task if you need to yield. Consult CDH before editing.	
		
		getxDirection();
		getyDirection();
		setxDuty();
		setyDuty();	
	}

}

// Static Helper Functions may be defined below.

/****************************************************************************************/
/* GET X DIRECTION													
 @Purpose: This function queries the X-axis current and voltage sensors on the solar 
			cells to get their most updated values. It then computes the new power by
			multiplying the values.
			The new power is compared to the last power measurement to determine if the 
			MPPT duty cycle should be adjusted in the same direction or if it should 
			switch directions															*/
/****************************************************************************************/
static void getxDirection(void)
{
	uint32_t pxv, pxi, pxp_new, pxp_last;

	pxv = get_sensor_data(PANELX_V);
	pxi = get_sensor_data(PANELX_I);
	pxp_new = pxi * pxi;

	if ((pxp_new < pxp_last) & xDirection == 1)
	{
		xDirection = 0;
	}
	else if ((pxp_new < pxp_last) & xDirection == 0)
	{
		xDirection = 1;
	}
	
	pxp_last = pxp_new;
}

/****************************************************************************************/
/* GET Y DIRECTION													
 @Purpose: This function queries the Y-axis current and voltage sensors on the solar 
			cells to get their most updated values. It then computes the new power by
			multiplying the values.
			The new power is compared to the last power measurement to determine if the 
			MPPT duty cycle should be adjusted in the same direction or if it should 
			switch directions															*/
/****************************************************************************************/
static void getyDirection(void)
{
	uint32_t pyv, pyi, pyp_new, pxp_new, pyp_last;

	pyv = get_sensor_data(PANELY_V);
	pyi = get_sensor_data(PANELY_I);
	pyp_new = pyi * pyi;

	if ((pyp_new < pyp_last) & yDirection == 1)
	{
		yDirection = 0;
	}
	
	else if ((pxp_new < pxp_last) & yDirection == 0)
	{
		yDirection = 1;
	}
	
	pyp_last = pyp_new;
}

/****************************************************************************************/
/* SET X DUTY													
 @Purpose: This function updates and sets the duty cycle that the MPPT is being run at
			This particular function sets for the X-axis MPPT and sends a CAN message  
			with the update to the EPS SSM												*/
/****************************************************************************************/
static uint8_t setxDuty(void)
{
	if (xDirection == 1)
	{
		xDuty = xDuty + DUTY_INCREMENT;
	}
	
	else if (xDirection == 0)
	{
		xDuty = xDuty - DUTY_INCREMENT;
	}
	set_variable_value(MPPTA, xDuty);	
}

/****************************************************************************************/
/* SET Y DUTY													
 @Purpose: This function updates and sets the duty cycle that the MPPT is being run at
			This particular function sets for the Y-axis MPPT and sends a CAN message  
			with the update to the EPS SSM												*/
/****************************************************************************************/
static uint8_t setyDuty(void)
{
	if (yDirection == 1)
	{
		yDuty = yDuty + DUTY_INCREMENT;
	}
	
	else if (yDirection == 0)
	{
		yDuty = yDuty - DUTY_INCREMENT;
	}
	set_variable(EPS_TASK_ID, EPS_ID, MPPTB, yDuty);
}


/****************************************************************************************/
/* SET UP MPPT													
 @Purpose: This function initializes all of the global variables used in running
			the MPPT algorithms									
																						*/
/****************************************************************************************/
static void setUpMPPT(void)
{
	pxp_last = 0;
	pyp_last = 0;
	xDirection = 0;
	yDirection = 0;
	xDuty = 0x3F;
	yDuty = 0x3F;
}

/************************************************************************/
/* GET SENSOR DATA														*/
/*																		*/
/* @param: sensor_id:	which sensor from the list in can_func.h		*/
/* @Purpose: This function wraps around request_sensor_data for EPS 
/*		specific sensor calls and deals with errors and resource waiting*/
/*																		*/
/* @return:								returns sensor value requested	*/
/* NOTE: This function will wait for a maximum of X * 25ms. for the		*/
/* operation to complete.												*/
/************************************************************************/
static uint32_t get_sensor_data(uint8_t sensor_id)
{
	//Declare testing variables
	int* status;
	uint8_t tries;
	uint32_t sensor_value;
	tries = 0;
	
	sensor_value = request_sensor_data(EPS_TASK_ID, EPS_ID, sensor_id, status);		//request a value
	if (*status == -1)								//If there is an error, check the status
	{
		tries++;									//Send an error if we have already tried too many times
		if (tries > MAX_NUM_TRIES)
		{
			tries = 0;
			//SEND SOME ERROR MESSAGE OF SOME KIND
		}
		else
		{
			request_sensor_data(EPS_TASK_ID, EPS_ID, sensor_id, status);		//Otherwise try again
		}
	}
	if (*status == 1)						//If status is 1 then we are good and we should return the sensor value 
	{
		return sensor_value;
	}
	else //SEND SOME ERROR MESSAGE OF SOME KIND
	{
		//Send error message b/c we shouldn't ever get here
	}
}

/****************************************************************************************/
/* SET VARIABLE VALUE													
																		
 @param: variable_name:	which variable from the list in can_func.h
 @param: new_var_value:	The new value that we would like to set for the variable 	
 
 @Purpose: This function wraps around the set_variable CAN API for setting EPS
			specific variables and deals with errors and resource waiting
																		
 @return: Nothing. it is a void function
 NOTE: This function will wait for a maximum of X * 25ms. for the		
 operation to complete.																	*/
/****************************************************************************************/

static void set_variable_value(uint8_t variable_name, uint8_t new_var_value)
{
	//Declare testing variables
	int status;
	uint8_t tries;
	tries = 0;
	
	status = set_variable(EPS_TASK_ID, EPS_ID, variable_name, new_var_value);
	if (status == -1)								//If there is an error, check the status
	{
		tries++;									//Send an error if we have already tried too many times
		if (tries > MAX_NUM_TRIES)
		{
			tries = 0;
			//SEND SOME ERROR MESSAGE OF SOME KIND
		}
		else
		{
			set_variable(EPS_TASK_ID, EPS_ID, variable_name, new_var_value);		//Otherwise try again
		}
	}
	if (status == 1)						//If status is 1 then we are good and we should return the sensor value
	{
		return;
	}
	else //SEND SOME ERROR MESSAGE OF SOME KIND
	{
		//Send error message b/c we shouldn't ever get here
	}
}