/*
Author: Samantha Murray, Keenan Burnett
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

/* Global variables */
#include "global_var.h" 
#include "error_handling.h"

/* Priorities at which the tasks are created. */
#define Eps_PRIORITY	( tskIDLE_PRIORITY + 1 ) // Lower the # means lower the priority
/* Values passed to the two tasks just to check the task parameter
functionality. */
#define EPS_PARAMETER	( 0xABCD )

/* Relevant Boundaries for power system		*/
#define MAX_NUM_TRIES			0xA
#define DUTY_INCREMENT			0x6
#define EPS_BALANCE_INTERVAL	0x2
#define EPS_HEATER_INTERVAL		0x5
/*-----------------------------------------------------------*/

/* Function Prototypes										 */
static void prvEpsTask( void *pvParameters );
TaskHandle_t eps(void);
void eps_kill(uint8_t killer);
static void getxDirection(void);
static void getyDirection(void);
static void setxDuty(void);
static void setyDuty(void);
static uint32_t get_sensor_data(uint8_t sensor_id);
static void set_variable_value(uint8_t variable_name, uint8_t new_var_value);
static void setUpMPPT(void);
static void battery_balance(void);
static void battery_heater(void);
static void verify_eps_sensor_value(uint8_t sensor_id);
static void init_eps_sensor_bounds(void);
static int send_event_report(uint8_t severity, uint8_t report_id, uint8_t num_params, uint32_t* data);
static void clear_current_command(void);
/*-----------------------------------------------------------*/


/* Global Variables Prototypes								*/

// For MPPT
static uint8_t xDirection, yDirection, xDuty, yDuty;
static uint32_t pxp_last, pyp_last;

// For Battery Balancing
static uint8_t last_balance_minute = 0;

// For Battery Heater
static uint8_t last_heater_control_minute = 0;
static uint32_t eps_target_temp, eps_temp_interval;

// Sensor values
static uint32_t battmv, battv, battin, battout;
static uint32_t epstemp, pxv, pyv, pxi, pyi;
static uint32_t comsv, comsi, payv, payi, obcv, obci;

// For the sensor boundaries 
static uint16_t pxv_high, pxv_low, pxi_high, pxi_low, pyv_high, pyv_low, pyi_high, pyi_low;
static uint16_t battmv_high, battmv_low, battv_high, battv_low, battin_high, battin_low;
static uint16_t battout_high, battout_low, epstemp_high, epstemp_low, comsv_high, comsv_low, comsi_high, comsi_low;
static uint16_t payv_high, payv_low, payi_high, payi_low, obcv_high, obcv_low, obci_high, obci_low;

// For the SEND_EVENT_REPORT service
static uint8_t current_command[DATA_LENGTH + 10];
/*-----------------------------------------------------------*/



/************************************************************************/
/* EPS (Function) 														*/
/* @Purpose: This function is simply used to create the EPS task below	*/
/* in main.c															*/
/************************************************************************/
TaskHandle_t eps( void )
{
	/* Start the two tasks as described in the comments at the top of this
	file. */
	TaskHandle_t temp_HANDLE = 0;
	xTaskCreate( prvEpsTask, /* The function that implements the task. */
		"ON", /* The text name assigned to the task - for debug only as it is not used by the kernel. */
		configMINIMAL_STACK_SIZE, /* The size of the stack to allocate to the task. */
		( void * ) EPS_PARAMETER, /* The parameter passed to the task - just to check the functionality. */
		Eps_PRIORITY, /* The priority assigned to the task. */
		&temp_HANDLE ); /* The task handle is not required, so NULL is passed. */

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached. If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks to be created. See the memory management section on the
	FreeRTOS web site for more details. */
	return temp_HANDLE;
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

	setUpMPPT();
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
		
		if ((CURRENT_MINUTE - last_balance_minute) > EPS_BALANCE_INTERVAL)
		{
			battery_balance();
		}
		if ((CURRENT_MINUTE - last_heater_control_minute) > EPS_HEATER_INTERVAL)
		{
			battery_heater();
		}
	}

}

// Static Helper Functions may be defined below.

/************************************************************************/
/* GET X DIRECTION														*/									
/* @Purpose: This function queries the X-axis current and voltage		*/
/*				sensors on the solar cells to get their most updated	*/
/*				values. It then computes the new power by multiplying	*/
/*				the values.												*/
/*				The new power is compared to the last power measurement */ 
/*				to determine if the MPPT duty cycle should be adjusted  */
/*				in the same direction or if it should switch directions	*/
/************************************************************************/
static void getxDirection(void)
{
	uint32_t pxv, pxi, pxp_new;

	pxv = get_sensor_data(PANELX_V);
	pxi = get_sensor_data(PANELX_I);
	pxp_new = pxi * pxv;

	if ((pxp_new < pxp_last) & (xDirection == 1))
	{
		xDirection = 0;
	}
	else if ((pxp_new < pxp_last) & (xDirection == 0))
	{
		xDirection = 1;
	}
	
	pxp_last = pxp_new;
}

/************************************************************************/
/* GET Y DIRECTION														*/
/* @Purpose: This function queries the Y-axis current and voltage		*/
/*				sensors on the solar cells to get their most updated	*/
/*				values. It then computes the new power by multiplying	*/
/*				the values.												*/
/*				The new power is compared to the last power measurement */
/*				to determine if the MPPT duty cycle should be adjusted  */
/*				in the same direction or if it should switch directions	*/
/************************************************************************/
static void getyDirection(void)
{
	uint32_t pyv, pyi, pyp_new;
	pyv = get_sensor_data(PANELY_V);
	pyi = get_sensor_data(PANELY_I);
	pyp_new = pyi * pyv;

	if ((pyp_new < pyp_last) & (yDirection == 1))
	{
		yDirection = 0;
	}
	
	else if ((pyp_new < pyp_last) & (yDirection == 0))
	{
		yDirection = 1;
	}
	
	pyp_last = pyp_new;
}

/************************************************************************/
/* SET X DUTY															*/
/* @Purpose: This function updates and sets the duty cycle that the		*/
/*				MPPT is being run atThis particular function sets for   */
/*				the X-axis MPPT and sends a CAN message with the		*/
/*				update to the EPS SSM									*/
/************************************************************************/
static void setxDuty(void)
{
	if (xDirection == 1)
	{
		xDuty = xDuty + DUTY_INCREMENT;
	}
	
	else if (xDirection == 0)
	{
		xDuty = xDuty - DUTY_INCREMENT;
	}
	set_variable_value(MPPTX, xDuty);	
}

/************************************************************************/
/* SET Y DUTY															*/
/* @Purpose: This function updates and sets the duty cycle that the		*/
/*				MPPT is being run atThis particular function sets for   */
/*				the X-axis MPPT and sends a CAN message with the		*/
/*				update to the EPS SSM									*/
/************************************************************************/
static void setyDuty(void)
{
	if (yDirection == 1)
	{
		yDuty = yDuty + DUTY_INCREMENT;
	}
	
	else if (yDirection == 0)
	{
		yDuty = yDuty - DUTY_INCREMENT;
	}
	set_variable_value(MPPTY, yDuty);
}


/************************************************************************/
/* SET UP MPPT															*/
/* @Purpose: This function initializes all of the global variables used */
/*				in running the MPPT algorithms							*/
/*																		*/
/************************************************************************/
static void setUpMPPT(void)
{
	pxp_last = 0xFFFFFFFF;
	pyp_last = 0xFFFFFFFF;
	xDirection = 0;
	yDirection = 0;
	xDuty = 0x3F;
	yDuty = 0x3F;
}

/************************************************************************/
/* GET SENSOR DATA														*/
/*																		*/
/* @param: sensor_id:	which sensor from the list in can_func.h		*/
/* @Purpose: This function wraps around request_sensor_data for EPS  	*/
/*				specific sensor calls and deals with errors and			*/
/*				resource waiting										*/
/*																		*/
/* @return:								returns sensor value requested	*/
/* NOTE: This function will wait for a maximum of X * 25ms. for the		*/
/* operation to complete.												*/
/************************************************************************/
static uint32_t get_sensor_data(uint8_t sensor_id)
{
	//Declare testing variables
	int* status = 0;
	uint8_t tries = 0;
	uint32_t sensor_value = 0;
	
	sensor_value = request_sensor_data(EPS_TASK_ID, EPS_ID, sensor_id, status);		//request a value
	while (*status == -1)								//If there is an error, check the status
	{
		if (tries++ > MAX_NUM_TRIES)
			{
				errorREPORT(EPS_TASK_ID, sensor_id, EPS_SSM_GET_SENSOR_DATA_ERROR, 0);
				return 0xFFFFFFFF;
			}
										
		else
			sensor_value = request_sensor_data(EPS_TASK_ID, EPS_ID, sensor_id, status);		//Otherwise try again
	}
	return sensor_value;
}

/************************************************************************/
/* SET VARIABLE VALUE													*/
/*																		*/
/* @param: variable_name:	which variable from the list in can_func.h	*/
/* @param: new_var_value:	New value that we want to set variable to 	*/
/*																		*/
/* @Purpose: This function wraps around the set_variable CAN API for 	*/
/*				setting EPS specific variables and deals with errors	*/
/*				and resource waiting									*/
/*																		*/
/* @return: Nothing. it is a void function								*/
/* NOTE: This function will wait for a maximum of X * 25ms. for the		*/
/* operation to complete.												*/
/************************************************************************/

static void set_variable_value(uint8_t variable_name, uint8_t new_var_value)
{
	//Declare testing variables
	int status;
	uint8_t tries = 0;
	status = set_variable(EPS_TASK_ID, EPS_ID, variable_name, new_var_value);
	while (status == 0xFF)								//If there is an error, check the status
	{
		if (tries++ > MAX_NUM_TRIES) {
			
			errorREPORT(EPS_TASK_ID, variable_name, EPS_SET_VARIABLE_ERROR, 0);
			return;
		}								// FAILURE_RECOVERY
		else
			status = set_variable(EPS_TASK_ID, EPS_ID, variable_name, new_var_value);		//Otherwise try again
	}
	return;						//If status is 1 then we are good and we should return the sensor value
}

/************************************************************************/
/* BATTERY_BALANCE														*/
/*																		*/
/* @Purpose: This function runns battery balancing for the eps		 	*/
/*				subsystem. It will only run when the batteries are		*/
/*				charging and it will currently only run every X minutes	*/
/*																		*/
/* NOTE: This function will wait for a maximum of X * 25ms. for the		*/
/* operation to complete.												*/
/************************************************************************/
static void battery_balance(void){
	//Declare variables
	uint32_t balance_l, balance_h, top_battery, bottom_battery;		//These are 8 bit values, but they are 32 here b/c that is what the set_sensor_data function returns

	// Timestamp the occurrence of this function
	last_balance_minute = CURRENT_MINUTE;

	//Update all the values we need to make decisions for battery balancing 
	balance_h = get_sensor_data(BALANCE_H);
	balance_l = get_sensor_data(BALANCE_L);
	battin = get_sensor_data(BATTIN_I);

	//Check if we should turn on battery balancing if it is currently off. 
	if ((balance_l = 0) && (balance_h = 0) && (battin > 2)) //battin > 2 corresponds to current into the battery if >8mA
	{
		//At this point, the conditions for battery balancing have been met so we want to check if we need to balance
		battv = get_sensor_data(BATT_V);	//min step size 0.0082V
		battmv = get_sensor_data(BATTM_V);	//min step size 0.0041V
		
		top_battery = battv-(2 * battmv);
		bottom_battery = (2 * battmv);	//For this, the min step is 
		
		if ((top_battery - bottom_battery) > 1) // a difference of 1 corresponds to ~8mV of difference in the battery voltages.  
		{
			set_variable_value(BALANCE_H, 1); // Turn on the balancing we want
		}
		if ((bottom_battery - top_battery) > 1)
		{
			set_variable_value(BALANCE_L, 1);
		}
	}
	
	//If balancing is currently on, check to see if we should turn it off
	if ((balance_l = 1) || (balance_h = 1)) //battin > 2 corresponds to current into the battery if >8mA
	{
		//At this point, the conditions for battery balancing have been met so we want to check if we need to balance
		battv = get_sensor_data(BATT_V);	//min step size 0.0082V
		battmv = get_sensor_data(BATTM_V);	//min step size 0.0041V
		
		top_battery = battv-(2 * battmv);
		bottom_battery = (2 * battmv);	//For this, the min step is now 0.0082V as well
		
		if ((top_battery - bottom_battery) <= 1) // a difference of 1 corresponds to ~8mV of difference in the battery voltages.
		{
			set_variable_value(BALANCE_H, 0); // Turn off balancing if the difference in cells is now small
		}
		if ((bottom_battery - top_battery) <= 1)
		{
			set_variable_value(BALANCE_L, 0); // Turn off balancing if the difference in cells is now small
		}
	}
}

/************************************************************************/
/* BATTERY_HEATER														*/
/*																		*/
/* @Purpose: This function decides when to turn on and off the battery	*/
/*				heater attached to the EPS subsystem. It will run with 	*/
/*				a hysteresis algorithm where the +/- from the center	*/
/*				and the set target value can be changed from the ground */
/*																		*/
/************************************************************************/
static void battery_heater(void){
	
	//Declare variables
	uint32_t batt_heater_control;

	// Timestamp the occurrence of this function
	last_heater_control_minute = CURRENT_MINUTE;

	//Update all the values we need to make decisions for battery heater
	epstemp = get_sensor_data(EPS_TEMP);
	batt_heater_control = get_sensor_data(BATT_HEAT);

	//Check if we should turn off the heater if it is currently on
	if ((batt_heater_control == 1) && (epstemp >= (eps_target_temp + eps_temp_interval)))
	{
		set_variable_value(BATT_HEAT, 0);
		//Report that we turned the heater off 
		send_event_report(1, BATTERY_HEATER_STATUS, 0, 0);
	}
		
	//Check if we should turn on the heater if it is currently off
	if ((batt_heater_control == 0) && (epstemp <= (eps_target_temp - eps_temp_interval)))
	{
		set_variable_value(BATT_HEAT, 1);
		//Report that we turned the heater on
		send_event_report(1, BATTERY_HEATER_STATUS, 0, 1);
	}
}

/************************************************************************/
/* VERIFY EPS SENSOR VALUE												*/
/* @param: sensor_id:	which sensor from the list in can_func.h		*/												
/* @Purpose: This function is to check that an individual sensor ID 	*/
/*				has a value within the set bounds						*/
/*																		*/
/* @return: Nothing. it is a void function								*/
/* NOTE: This function will wait for a maximum of X * 25ms. for the		*/
/* operation to complete.												*/
/************************************************************************/

void verify_eps_sensor_value(uint8_t sensor_id)
{
	uint32_t* val = 0;
	*val = sensor_id;
	switch(sensor_id){
		//NEED TO ADD IN A REPORTING FUNCTION THAT SOMEHOW NOTIFIES US
		case PANELX_V :
			if ((pxv < pxv_low) || (pxv > pxv_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case PANELX_I :
			if ((pxi < pxi_low) || (pxi > pxi_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case PANELY_V :
			if ((pyv < pyv_low) || (pyv > pyv_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case PANELY_I :
			if ((pyi < pyi_low) || (pyi > pyi_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case BATTM_V :
			if ((battmv < battmv_low) || (battmv > battmv_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case BATT_V :
			if ((battv < battv_low) || (battv > battv_low))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case BATTIN_I :
			if ((battin < battin_low) || (battin > battin_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case BATTOUT_I :
			if ((battout < battout_low) || (battout > battout_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case EPS_TEMP :
			if ((epstemp < epstemp_low) || (epstemp > epstemp_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case COMS_V	:
			if ((comsv < comsv_low) || (comsv > comsv_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case COMS_I :
			if ((comsi < comsi_low) || (comsi > comsi_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case PAY_V :
			if ((payv < payv_low) || (payv > payv_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case PAY_I :
			if ((payi < payi_low) || (payi > payi_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case OBC_V :
			if ((obcv < obcv_low) || (obcv > obcv_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		case OBC_I :
			if ((obci < obci_low) || (obci > obcv_high))
			{
				send_event_report(1, EPS_SENSOR_VALUE_OUT_OF_RANGE, 1, val);
			}
			break;
		default :
			break;
	}
}

/************************************************************************/
/* INIT EPS SENSOR BOUNDS												*/
/*																		*/
/* @Purpose: This function is to create the boundaries for the EPS		*/
/*				sensor readings so that FDIR can be notified if one of	*/
/*				these bounds is outside of the reading it should be.	*/
/*																		*/					
/* @return: Nothing. it is a void function								*/
/* NOTE: This function will wait for a maximum of X * 25ms. for the		*/		
/* operation to complete.												*/
/************************************************************************/

void init_eps_sensor_bounds(void){
	// Set up all the values to 0 and highest for now before I calculate the actual values. 
	pxv_high = 0xFFFF;
	pxv_low = 0x0000;
	
	pxi_high = 0xFFFF;
	pxi_low = 0x0000;
	
	pyv_high = 0xFFFF;
	pyv_low = 0x0000;
	
	pyi_high = 0xFFFF;
	pyi_low = 0x0000;
	
	battmv_high = 0xFFFF;
	battmv_low = 0x0000;
	
	battv_high = 0xFFFF;
	battv_low = 0x0000;
	
	battin_high = 0xFFFF;
	battin_low = 0x0000;
	
	battout_high = 0xFFFF;
	battout_low = 0x0000;
	
	epstemp_high = 0xFFFF;
	epstemp_low = 0x0000;
	
	comsv_high = 0xFFFF;
	comsv_low = 0x0000;
	
	comsi_high = 0xFFFF;
	comsi_low = 0x0000;
	
	payv_high = 0xFFFF;
	payv_low = 0x0000;
	
	payi_high = 0xFFFF;
	payi_low = 0x0000;
	
	obcv_high = 0xFFFF;
	obcv_low = 0x0000;
	
	obci_high = 0xFFFF;
	obci_low = 0x0000;
	return;
}

/************************************************************************/
/* SEND_EVENT_REPORT		                                            */
/* @Purpose: sends a message to the OBC_PACKET_ROUTER using				*/
/* sched_to_obc_fifo. The intent is to an event report downlinked to	*/
/* the ground station.													*/
/* @param: severity: 1 = Normal. (2|3|4) = increasing sev of error		*/
/* @param: report_id: Unique to the event report, ex: BIT_FLIP_DETECTED */
/* @param: num_params: The number of 32-bit parameters that are being	*/
/* transmitted to ground.												*/
/* @param: *data: A pointer to where the data for the parameters is		*/
/* currently stored.													*/
/* @Note: This PUS packet shall be left-adjusted.						*/
/************************************************************************/
static int send_event_report(uint8_t severity, uint8_t report_id, uint8_t num_params, uint32_t* data)
{
	clear_current_command();
	if(num_params > 34)
		return -1;		// Invalid number of parameters.
	current_command[146] = TASK_TO_OPR_EVENT;
	current_command[145] = severity;
	current_command[136] = report_id;
	current_command[135] = num_params;
	for(uint8_t i = 0; i < num_params; i++)
	{
		current_command[134 - (i * 4)]		= (uint8_t)((*(data + i) & 0xFF000000) >> 24);
		current_command[134 - (i * 4) - 1]	= (uint8_t)((*(data + i) & 0x00FF0000) >> 16);
		current_command[134 - (i * 4) - 2]	= (uint8_t)((*(data + i) & 0x0000FF00) >> 8);
		current_command[134 - (i * 4) - 3]	= (uint8_t)(*(data + i) & 0x000000FF);
	}
	xQueueSendToBack(eps_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY
	return 1;
}

// This function will kill this task.
// If it is being called by this task 0 is passed, otherwise it is probably the FDIR task and 1 should be passed.
void eps_kill(uint8_t killer){
	// Free the memory that this task allocated.
	//vPortFree(xDirection);
	//vPortFree(yDirection);
	//vPortFree(xDuty);
	//vPortFree(yDuty);
	//vPortFree(pxp_last);
	//vPortFree(pyp_last);
	//vPortFree(battmv);
	//vPortFree(battv);
	//vPortFree(battin);
	//vPortFree(battout);
	//vPortFree(epstemp);
	//vPortFree(comsv);
	//vPortFree(comsi);
	//vPortFree(payv);
	//vPortFree(payi);
	//vPortFree(obcv);
	//vPortFree(obci);
	// Kill the task.
	if(killer)
		vTaskDelete(eps_HANDLE);
	else
		vTaskDelete(NULL);
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
