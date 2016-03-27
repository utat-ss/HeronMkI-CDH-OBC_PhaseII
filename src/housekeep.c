/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		housekeep.c
	*
	*	PURPOSE:		
	*	This file is to be used to create the housekeeping task needed to monitor
	*	housekeeping information on the satellite.
	*
	*	FILE REFERENCES:	stdio.h, FreeRTOS.h, task.h, partest.h, asf.h, can_func.h, spimem.h, error_handling.h
	*
	*	EXTERNAL VARIABLES:
	*
	*	EXTERNAL REFERENCES:	Same a File References.
	*
	*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
	*
	*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
	*
	*	NOTES:
	*			When housekeeping or diagnostics are immediately requested, new values HAVE to be sampled.
	*
	*			Consider thinking about how to guarantee that every time housekeeping is downlinked those
	*			values are fresh.
	*	
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:			
	*	New tasks should be written to use as much of CMSIS as possible. The ASF and 
	*	FreeRTOS API libraries should also be used whenever possible to make the program
	*	more portable.

	*	DEVELOPMENT HISTORY:		
	*	02/18/2015		Created.
	*
	*	08/09/2015		Added the function read_from_SSM() to this tasks's duties as a demonstration
	*					of our reprogramming capabilities.
	*
	*	11/04/2015		I am adding in functionality so that we periodically send a housekeeping packet
	*					to obc_packet_router through hk_to_t_fifo.
	*					HK "packets" are just the data field of the PUS packet to be sent and are hence
	*					a maximum of 128 B. They can be larger, but then they must be split into multiple
	*					packets.
	*
	*	11/05/2015		I added a bunch of helper functions in order to implement what is required of this task
	*					in order to fulfill the PUS service specifications.
	*
	*					These functions include: store_housekeeping(), send_hk_as_tm(), send_params_report(), and exec_commands().
	*
	*	11/07/2015		I am also adding some functionality into store_housekeeping() so that it keeps a certain number of
	*					old housekeeping packets in SPI memory.
	*
	*					The current offset of the housekeeping log needs to be stored in SPI memory just in case the main
	*					computer gets reset (we would lose track of how much housekeeping is currently in memory)
	*	
	*	12/05/2015		A:Added error handling for exec_commands, store_housekeeping using wrapper functions
	*
	*	12/09/2015		Added in housekeep_suicide() so that this task can kill itself if need be (or if commanded by the fdir task).
	*
	*   01/15/2016      A:Added in a wrapper function for FIFO error handling in xQueueSendToBack
	*	DESCRIPTION:
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

/* CAN Function includes */
#include "can_func.h"

/* SPI memory includes	 */
#include "spimem.h"

/* Error Handling includes */
#include "error_handling.h"

#include "global_var.h"

/* Priorities at which the tasks are created. */
#define Housekeep_PRIORITY		( tskIDLE_PRIORITY + 1 )		// Lower the # means lower the priority

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define HK_PARAMETER			( 0xABCD )

/* HK DEFINITION DEFINES		*/
#define DEFAULT					0
#define ALTERNATE				1

#define HK_LOOP_TIMEOUT			15000							// Specifies how many ticks to wait before running housekeeping again.

/* Definitions to clarify which service subtypes represent what	*/
/* Housekeeping							
#define NEW_HK_DEFINITION				1
#define CLEAR_HK_DEFINITION				3
#define ENABLE_PARAM_REPORT				5
#define DISABLE_PARAM_REPORT			6
#define HK_DEFINITON_REPORT				10
#define HK_REPORT						25
-----------------------------------------------------------*/

/* Function Prototypes */
static void prvHouseKeepTask( void *pvParameters );
TaskHandle_t housekeep(void);
void housekeep_kill(uint8_t killer);
static void clear_current_hk(void);
static int request_housekeeping_all(void);
static int store_housekeeping(void);
//static int store_housekeeping_H(void);
static void setup_default_definition(void);
static void set_definition(uint8_t sID);
static void clear_alternate_hk_definition(void);
static void clear_current_command(void);
static void send_hk_as_tm(void);
static void send_param_report(void);
static void exec_commands(void);
static int exec_commands_H(void);
static int exec_commands_H2(void);
static int store_hk_in_spimem(void);
static void set_hk_mem_offset(void);
static void send_tc_execution_verify(uint8_t status, uint16_t packet_id, uint16_t psc);

uint8_t get_ssm_id(uint8_t sensor_name);


/* Global Variables for Housekeeping */
static uint8_t current_hk[DATA_LENGTH];				// Used to store the next housekeeping packet we would like to downlink.
static uint8_t current_command[DATA_LENGTH + 10];	// Used to store commands which are sent from the OBC_PACKET_ROUTER.
static uint8_t hk_definition0[DATA_LENGTH];			// Used to store the current housekeeping format definition.
static uint8_t hk_definition1[DATA_LENGTH];			// Used to store an alternate housekeeping definition.
static uint8_t hk_updated[DATA_LENGTH];
static uint8_t current_hk_definition[DATA_LENGTH];
static uint8_t current_hk_definitionf;					// Unique identifier for the housekeeping format definition.
static uint8_t current_eps_hk[DATA_LENGTH / 4], current_coms_hk[DATA_LENGTH / 4], current_pay_hk[DATA_LENGTH / 4], current_obc_hk[DATA_LENGTH / 4];
static uint32_t new_hk_msg_high, new_hk_msg_low;
static uint8_t current_hk_fullf, param_report_requiredf;
static uint8_t collection_interval0, collection_interval1;		// How to wait for the next collection (in minutes)
static TickType_t xTimeToWait;				// Number entered here corresponds to the number of ticks we should wait.
static uint8_t current_hk_mem_offset[4];
static TickType_t	xLastWakeTime;
int req_data_result;

/************************************************************************/
/* HOUSEKEEPING (Function) 												*/
/* @Purpose: This function is used to create the housekeeping task.		*/
/************************************************************************/
TaskHandle_t housekeep( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		TaskHandle_t temp_HANDLE = 0;
		xTaskCreate( prvHouseKeepTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) HK_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					Housekeep_PRIORITY, 			/* The priority assigned to the task. */
					&temp_HANDLE );								/* The task handle is not required, so NULL is passed. */
					
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	return temp_HANDLE;
}

/************************************************************************/
/*				HOUSEKEEPING TASK		                                */
/* This task is used to periodically gather housekeeping information	*/
/* and store it in external memory until it is ready to be downlinked.  */
/************************************************************************/
static void prvHouseKeepTask(void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == HK_PARAMETER );
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
	new_hk_msg_high = 0;
	new_hk_msg_low = 0;
	current_hk_fullf = 0;
	current_hk_definitionf = 0;		// Default definition
	param_report_requiredf = 0;
	collection_interval0 = 30;
	collection_interval1 = 30;
	xTimeToWait = 10;
	TickType_t last_tick_count = xTaskGetTickCount();

	clear_current_hk();
	clear_current_command();
	setup_default_definition();
	set_definition(DEFAULT);
	clear_alternate_hk_definition();
	//set_hk_mem_offset();
		
	/* @non-terminating@ */	
	for( ;; )
	{
		if(xTaskGetTickCount() - last_tick_count > HK_LOOP_TIMEOUT)
		{
			if(request_housekeeping_all())
			{
				store_housekeeping();
				send_hk_as_tm();
				last_tick_count = xTaskGetTickCount();
			}
		}
		exec_commands();
		if(param_report_requiredf)
			send_param_report();
	}
}
/*-----------------------------------------------------------*/
/* static helper functions below */

/************************************************************************/
/* EXEC_COMMANDS														*/
/* @Purpose: Attempts to receive from obc_to_hk_fifo, executes			*/
/* different commands depending on what was received.					*/
/************************************************************************/
static void exec_commands(void){
	int attempts = 1;
	uint8_t exec_com_success = (uint8_t)exec_commands_H(); 	//exec_com_success is 1 if successful, current_commands if there is a FIFO error
	/* Error Handling */
	//while (attempts<3 && exec_com_success != 1){
		//exec_com_success = (uint8_t)exec_commands_H();
		//attempts++;
	//}
	//if (exec_com_success != 1) {
		//errorREPORT(HK_TASK_ID,0,HK_FIFO_RW_ERROR, &exec_com_success);
	//}
	/******************/
	return;
}

// Will return 1 if successful, current_command if there is a FIFO error/

static int exec_commands_H(void)
{
	clear_current_command();
	if(xQueueReceive(obc_to_hk_fifo, current_command, (TickType_t)1) == pdTRUE)
		return exec_commands_H2();
	else if(xQueueReceive(sched_to_hk_fifo, current_command, (TickType_t)1))
		return exec_commands_H2();
	else										//Failure Recovery					
		return 0;
}

static int exec_commands_H2(void)
{
	uint8_t i, command;
	uint16_t packet_id, psc;
	packet_id = ((uint16_t)current_command[140]) << 8;
	packet_id += (uint16_t)current_command[139];
	psc = ((uint16_t)current_command[138]) << 8;
	psc += (uint16_t)current_command[137];
	command = current_command[146];
	switch(command)
	{
		case	NEW_HK_DEFINITION:
			collection_interval1 = current_command[145];
			for(i = 0; i < DATA_LENGTH; i++)
			{
				hk_definition1[i] = current_command[i];
			}
			hk_definition1[136] = 1;		//sID = 1
			hk_definition1[135] = collection_interval1;
			hk_definition1[134] = current_command[146];
			set_definition(ALTERNATE);
			send_tc_execution_verify(1, packet_id, psc);		// Send TC Execution Verification (Success)
		case	CLEAR_HK_DEFINITION:
			collection_interval1 = 30;
			for(i = 0; i < DATA_LENGTH; i++)
			{
				hk_definition1[i] = 0;
			}
			set_definition(DEFAULT);
			send_tc_execution_verify(1, packet_id, psc);
		case	ENABLE_PARAM_REPORT:
			param_report_requiredf = 1;
			send_tc_execution_verify(1, packet_id, psc);
		case	DISABLE_PARAM_REPORT:
			param_report_requiredf = 0;
			send_tc_execution_verify(1, packet_id, psc);
		case	REPORT_HK_DEFINITIONS:
			param_report_requiredf = 1;
			//send_tc_execution_verify(1, packet_id, psc);
		default:
			return -1;
	}
	return 1;
}

/************************************************************************/
/* CLEAR_CURRENT_HK														*/
/* @Purpose: clears the array current_hk[]								*/
/************************************************************************/
static void clear_current_hk(void)
{
	uint8_t i;
	for(i = 0; i < DATA_LENGTH; i++)
	{
		current_hk[i] = 0;
		hk_updated[i] = 0;
	}
	return;
}

/************************************************************************/
/* SET_HK_MEM_OFFSET													*/
/* @Purpose: sets the memory offset for HK's reading/writing to SPIMEM	*/
/************************************************************************/
static void set_hk_mem_offset(void)
{
	uint32_t offset;
	spimem_read(HK_BASE, current_hk_mem_offset, 4);	// Get the current HK memory offset.
	offset = (uint32_t)(current_hk_mem_offset[0] << 24);
	offset += (uint32_t)(current_hk_mem_offset[1] << 16);
	offset += (uint32_t)(current_hk_mem_offset[2] << 8);
	offset += (uint32_t)current_hk_mem_offset[3];
	
	if(offset == 0)
		current_hk_mem_offset[3] = 4;
		spimem_write(HK_BASE + 3, current_hk_mem_offset + 3, 1);
	
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
/* CLEAR_ALTERNATE_HK_DEFINITION										*/
/* @Purpose: clears the array current_hk_definition						*/
/************************************************************************/
static void clear_alternate_hk_definition(void)
{
	uint8_t i;
	for(i = 0; i < DATA_LENGTH; i++)
	{
		hk_definition1[i] = 0;
	}
	return;
}

/************************************************************************/
/* REQUEST_HOUSEKEEPING_ALL												*/
/* @Purpose: Requests for housekeeping from each SSM.					*/
/* @return: 1 = action succeeded, -1 = something bad happened.			*/
/************************************************************************/
static int request_housekeeping_all(void)
{			
	if(request_housekeeping(EPS_ID) > 1)							// Request housekeeping from COMS.
		return -1;
	if(request_housekeeping(COMS_ID) > 1)							// Request housekeeping from EPS.
		return -1;
	if(request_housekeeping(PAY_ID) > 1)							// Request housekeeping from PAY.
		return -1;
	return 1;
}

/************************************************************************/
/* STORE_HOUSEKEEPING													*/
/* @Purpose: Reads from the housekeeping fifo, and places incoming		*/
/* parameters into curren_hk[]											*/
/* @Note: This function erases old values but also keeps track of values*/
/* were not updated, and subsequently sends an event report to ground	*/
/* if one was updated as well as a message to the FDIR task.			*/
/************************************************************************/
static int store_housekeeping(void)
{
	uint8_t sender = 0xFF;
	uint8_t num_parameters = current_hk_definition[129];	// ALTERED FOR CSDC 134 --> 129
	uint8_t parameter_name = 0, i;
	//int attempts = 1;
	int* status = 0; // this might be wrong
	req_data_result = 0;
	if(current_hk_fullf)
		return -1;
	for(i = 0; i < PACKET_LENGTH; i++)
	{
		hk_updated[i] = 0;
	}
	uint8_t parameter_count = num_parameters;
	uint32_t timeout = 5000;
	//clear_current_hk();									// Commented out for CSDC
	
	// CSDC ONLY  // (Filling in OBC values for housekeeping)
	current_hk[94] = 0x55;
	current_hk[93] = 0x55;
	current_hk[83] = ABSOLUTE_DAY;
	current_hk[81] = CURRENT_HOUR;
	current_hk[79] = CURRENT_MINUTE;
	hk_updated[94] = 1;
	hk_updated[93] = 1;
	hk_updated[84] = 1;
	hk_updated[83] = 1;
	hk_updated[82] = 1;
	hk_updated[81] = 1;
	hk_updated[80] = 1;
	hk_updated[79] = 1;
	
	while(parameter_count && timeout--)
	{
		if(read_can_hk(&new_hk_msg_high, &new_hk_msg_low, 1234) == 1)
		{
			parameter_count--;
			parameter_name = (new_hk_msg_high & 0x00000FF);	// Name of the parameter for housekeeping (either sensor or variable).
			for(i = 79; i < 79 + num_parameters * 2; i+=2)					// ALTERED FOR CSDC (i = 0 before)
			{
				if(current_hk_definition[i] == parameter_name)
				{
					current_hk[i] = (uint8_t)(new_hk_msg_low & 0x000000FF);
					current_hk[i + 1] = (uint8_t)((new_hk_msg_low & 0x0000FF00) >> 8);
					hk_updated[i] = 1;
					hk_updated[i + 1] = 1;
				}
			}			
		}
		taskYIELD();		// Allows for more messages to come in.
	}
	
	for(i = 79; i < 79 + num_parameters * 2; i+=2)							// ALTERED FOR CSDC (i = 0 before)
	{
		if(!hk_updated[i])
		{//failed updates are requested 3 times. If they fail, error is reported
			
			req_data_result = (int)request_sensor_data(HK_TASK_ID,get_ssm_id(current_hk_definition[i]),current_hk_definition[i],status);
			//while (attempts < 3 && req_data_result == -1){
				//attempts++;
				//req_data_result = (int)request_sensor_data(HK_TASK_ID,get_ssm_id(current_hk_definition[i]),current_hk_definition[i],status);
			//}
			
			if (*status == -1)
			{
				//errorREPORT(HK_TASK_ID,0,HK_COLLECT_ERROR, current_hk_definition[i]); 				//malfunctioning sensor is sent to erorREPORT
			}
			else {
				current_hk[i] = (uint8_t)(req_data_result & 0x000000FF);
				current_hk[i + 1] = (uint8_t)((req_data_result & 0x0000FF00) >> 8);
				hk_updated[i] = 1;
				hk_updated[i + 1] = 1;
			}
		}
	}
	/* Store the new housekeeping in SPI memory */
	//store_hk_in_spimem();				// Removed for CSDC
	
	current_hk_fullf = 1;
	return 1;
}

//Returns the proper ssm_id for a given sensor/variable
uint8_t get_ssm_id(uint8_t sensor_name)
{
	//for sensors://
	if ((sensor_name>=0x01 && sensor_name <=0x11)||(sensor_name == 0xFF) || (sensor_name == 0xFC) || (sensor_name == 0xFE))
		return EPS_ID;
	if ((sensor_name == 0x12) || (sensor_name == 0xFD))
		return COMS_ID;
	if ((sensor_name == 0x13) || (sensor_name == 0xFA) || (sensor_name>=0xF2 && sensor_name <=0xF8))
		return OBC_ID; //not sure if this is right
	if ((sensor_name>0x13 && sensor_name <= 0x63) || (sensor_name == 0xFB) || (sensor_name == 0xF9))
		return PAY_ID;
	//for global variables:
	if ((sensor_name == MPPTX) || (sensor_name == MPPTY) || (sensor_name == EPS_MODE) || (sensor_name == EPS_FDIR_SIGNAL) || 
		(sensor_name == BALANCE_H) || (sensor_name == BALANCE_L) || (sensor_name == BATT_HEAT) || (sensor_name == EPS_BAL_INTV)
		 || (sensor_name == EPS_HEAT_INTV) || (sensor_name == EPS_TRGT_TMP) || (sensor_name == EPS_TEMP_INTV))
		return EPS_ID;
	if ((sensor_name == COMS_MODE) || (sensor_name == SSM_CTT) || (sensor_name == SSM_OGT) || (sensor_name == COMS_FDIR_SIGNAL))
		return COMS_ID;
	if ((sensor_name == PAY_MODE) || (sensor_name == PAY_STATE) || (sensor_name == PAY_FDIR_SIGNAL))
		return PAY_ID;
	if ((sensor_name == OBC_MODE) || (sensor_name == ABS_TIME_D) || (sensor_name == ABS_TIME_H) || (sensor_name == ABS_TIME_M) || 
		(sensor_name == ABS_TIME_S) || (sensor_name == SPI_CHIP_1) || (sensor_name == SPI_CHIP_2) || (sensor_name == SPI_CHIP_3) || 
		(sensor_name == OBC_CTT) || (sensor_name == OBC_OGT))
		return OBC_ID;
	//assume the worst:
	return OBC_ID;
	
}

/************************************************************************/
/* STORE_HK_IN_SPIMEM													*/
/* @Purpose: stores the housekeeping which was just collected in spimem	*/
/* along with a timestamp.												*/
/* @Note: When housekeeping fills up, this function simply wraps back	*/
/* around to the beginning of housekeeping in memory.					*/
/************************************************************************/
static int store_hk_in_spimem(void)
{
	uint32_t offset;
	int x;
	offset = (uint32_t)(current_hk_mem_offset[0] << 24);
	offset += (uint32_t)(current_hk_mem_offset[1] << 16);
	offset += (uint32_t)(current_hk_mem_offset[2] << 8);
	offset += (uint32_t)current_hk_mem_offset[3];

	//task_spimem_write(HK_TASK_ID, (HK_BASE + offset), absolute_time_arr, 4);		// Write the timestamp and then the housekeeping
	//task_spimem_write(HK_TASK_ID, (HK_BASE + offset + 4), &current_hk_definitionf, 1);	// Writes the sID to memory.
	//x = task_spimem_write(HK_TASK_ID, (HK_BASE + offset + 5), current_hk, 128);		// FAILURE_RECOVERY if x < 0
	if(x < 0)
		return x;
	offset = (offset + 137) % LENGTH_OF_HK;								// Make sure HK doesn't overflow into the next section.

	if(offset == 0)
		offset = 4;
	current_hk_mem_offset[3] = (uint8_t)offset;
	current_hk_mem_offset[2] = (uint8_t)((offset & 0x0000FF00) >> 8);
	current_hk_mem_offset[1] = (uint8_t)((offset & 0x00FF0000) >> 16);
	current_hk_mem_offset[0] = (uint8_t)((offset & 0xFF000000) >> 24);
	//return 	task_spimem_write(HK_TASK_ID, HK_BASE, current_hk_mem_offset, 4);			// FAILURE_RECOVERY if x < 0
	return 1;
}

/************************************************************************/
/* SETUP_DEFAULT_DEFINITION												*/
/* @Purpose: This function will set the values stored in the default	*/
/* housekeeping definition and will also set it as the current hk def	*/
/************************************************************************/
static void setup_default_definition(void)
{
	uint8_t i;
	
	for(i = 0; i < DATA_LENGTH; i++)
	{
		hk_definition0[i] = 0;
	}
	
	//hk_definition0[136] = 0;							// sID = 0
	//hk_definition0[135] = collection_interval0;			// Collection interval = 30 min
	//hk_definition0[134] = 36;							// Number of parameters (2B each)
	//hk_definition0[81] = PANELX_V;
	//hk_definition0[80] = PANELX_V;
	//hk_definition0[79] = PANELX_I;
	//hk_definition0[78] = PANELX_I;
	//hk_definition0[77] = PANELY_V;
	//hk_definition0[76] = PANELY_V;
	//hk_definition0[75] = PANELY_I;
	//hk_definition0[74] = PANELY_I;
	//hk_definition0[73] = BATTM_V;
	//hk_definition0[72] = BATTM_V;
	//hk_definition0[71] = BATT_V;
	//hk_definition0[70] = BATT_V;
	//hk_definition0[69] = BATTIN_I;
	//hk_definition0[68] = BATTIN_I;
	//hk_definition0[67] = BATTOUT_I;
	//hk_definition0[66] = BATTOUT_I;
	//hk_definition0[65] = BATT_TEMP;
	//hk_definition0[64] = BATT_TEMP;	//
	//hk_definition0[63] = EPS_TEMP;
	//hk_definition0[62] = EPS_TEMP;	//
	//hk_definition0[61] = COMS_V;
	//hk_definition0[60] = COMS_V;
	//hk_definition0[59] = COMS_I;
	//hk_definition0[58] = COMS_I;
	//hk_definition0[57] = PAY_V;
	//hk_definition0[56] = PAY_V;
	//hk_definition0[55] = PAY_I;
	//hk_definition0[54] = PAY_I;
	//hk_definition0[53] = OBC_V;
	//hk_definition0[52] = OBC_V;
	//hk_definition0[51] = OBC_I;
	//hk_definition0[50] = OBC_I;
	//hk_definition0[49] = SHUNT_DPOT;
	//hk_definition0[48] = SHUNT_DPOT;
	//hk_definition0[47] = COMS_TEMP;	//
	//hk_definition0[46] = COMS_TEMP;
	//hk_definition0[45] = OBC_TEMP;	//
	//hk_definition0[44] = OBC_TEMP;
	//hk_definition0[43] = PAY_TEMP0;
	//hk_definition0[42] = PAY_TEMP0;
	//hk_definition0[41] = PAY_TEMP1;
	//hk_definition0[40] = PAY_TEMP1;
	//hk_definition0[39] = PAY_TEMP2;
	//hk_definition0[38] = PAY_TEMP2;
	//hk_definition0[37] = PAY_TEMP3;
	//hk_definition0[36] = PAY_TEMP3;
	//hk_definition0[35] = PAY_TEMP4;
	//hk_definition0[34] = PAY_TEMP4;
	//hk_definition0[33] = PAY_HUM;
	//hk_definition0[32] = PAY_HUM;
	//hk_definition0[31] = PAY_PRESS;
	//hk_definition0[30] = PAY_PRESS;
	//hk_definition0[29] = PAY_ACCEL;
	//hk_definition0[28] = PAY_ACCEL;
	//hk_definition0[27] = MPPTX;
	//hk_definition0[26] = MPPTX;
	//hk_definition0[25] = MPPTY;
	//hk_definition0[24] = MPPTY;
	//hk_definition0[23] = COMS_MODE;	//
	//hk_definition0[22] = COMS_MODE;
	//hk_definition0[21] = EPS_MODE;	//
	//hk_definition0[20] = EPS_MODE;
	//hk_definition0[19] = PAY_MODE;
	//hk_definition0[18] = PAY_MODE;
	//hk_definition0[17] = OBC_MODE;
	//hk_definition0[16] = OBC_MODE;
	//hk_definition0[15] = PAY_STATE;
	//hk_definition0[14] = PAY_STATE;
	//hk_definition0[13] = ABS_TIME_D;
	//hk_definition0[12] = ABS_TIME_D;
	//hk_definition0[11] = ABS_TIME_H;
	//hk_definition0[10] = ABS_TIME_H;
	//hk_definition0[9] = ABS_TIME_M;
	//hk_definition0[8] = ABS_TIME_M;
	//hk_definition0[7] = ABS_TIME_S;
	//hk_definition0[6] = ABS_TIME_S;
	//hk_definition0[5] = SPI_CHIP_1;
	//hk_definition0[4] = SPI_CHIP_1;
	//hk_definition0[3] = SPI_CHIP_2;
	//hk_definition0[2] = SPI_CHIP_2;
	//hk_definition0[1] = SPI_CHIP_3;
	//hk_definition0[0] = SPI_CHIP_3;
	
	/* The definition below is meant to be used in the CSDC environmental testing */
	
	hk_definition0[131] = 0;							// sID = 0
	hk_definition0[130] = collection_interval0;			// Collection interval = 30 min
	hk_definition0[129] = 25;							// Number of parameters (2B each)
	hk_definition0[128] = PANELX_V;
	hk_definition0[127] = PANELX_V;
	hk_definition0[126] = PANELX_I;
	hk_definition0[125] = PANELX_I;
	hk_definition0[124] = PANELY_V;
	hk_definition0[123] = PANELY_V;
	hk_definition0[122] = PANELY_I;
	hk_definition0[121] = PANELY_I;
	hk_definition0[120] = BATTM_V;
	hk_definition0[119] = BATTM_V;
	hk_definition0[118] = BATT_V;
	hk_definition0[117] = BATT_V;
	hk_definition0[116] = BATTIN_I;
	hk_definition0[115] = BATTIN_I;
	hk_definition0[114] = BATTOUT_I;
	hk_definition0[113] = BATTOUT_I;
	hk_definition0[112] = BATT_TEMP;
	hk_definition0[111] = BATT_TEMP;	//
	hk_definition0[110] = EPS_TEMP;
	hk_definition0[109] = EPS_TEMP;	//
	hk_definition0[108] = COMS_V;
	hk_definition0[107] = COMS_V;
	hk_definition0[106] = COMS_I;
	hk_definition0[105] = COMS_I;
	hk_definition0[104] = PAY_V;
	hk_definition0[103] = PAY_V;
	hk_definition0[102] = PAY_I;
	hk_definition0[101] = PAY_I;
	hk_definition0[100] = OBC_V;
	hk_definition0[99] = OBC_V;
	hk_definition0[98] = OBC_I;
	hk_definition0[97] = OBC_I;
	hk_definition0[96] = COMS_TEMP;	//
	hk_definition0[95] = COMS_TEMP;
	hk_definition0[94] = OBC_TEMP;	//
	hk_definition0[93] = OBC_TEMP;
	hk_definition0[92] = PAY_TEMP0;
	hk_definition0[91] = PAY_TEMP0;
	hk_definition0[90] = PAY_HUM;
	hk_definition0[89] = PAY_HUM;
	hk_definition0[88] = PAY_PRESS;
	hk_definition0[87] = PAY_PRESS;
	hk_definition0[86] = PAY_ACCEL;
	hk_definition0[85] = PAY_ACCEL;
	hk_definition0[84] = ABS_TIME_D;
	hk_definition0[83] = ABS_TIME_D;
	hk_definition0[82] = ABS_TIME_H;
	hk_definition0[81] = ABS_TIME_H;
	hk_definition0[80] = ABS_TIME_M;
	hk_definition0[79] = ABS_TIME_M;
	return;
}



/************************************************************************/
/* SENT_DEFINITION														*/
/* @Purpose: This function will change which definition is being used	*/
/* for creating housekeeping reports.									*/
/* @param: sID: 0 == default, 1 = alternate.							*/
/************************************************************************/
static void set_definition(uint8_t sID)
{
	uint8_t i;
	if(!sID)								// DEFAULT
	{
		for(i = 0; i < DATA_LENGTH; i++)
		{
			current_hk_definition[i] = hk_definition0[i];		
		}
		current_hk_definitionf = 0;
		xTimeToWait = collection_interval0 * 1000 * 60;
	}
	if(sID == 1)							// ALTERNATE
	{
		for(i = 0; i < DATA_LENGTH; i++)
		{
			current_hk_definition[i] = hk_definition1[i];
		}
		current_hk_definitionf = 1;
		xTimeToWait = collection_interval1 * 1000 * 60;
	}
	return;
}

/************************************************************************/
/* SEND_HK_AS_TM														*/
/* @Purpose: This function will downlink all housekeeping to ground		*/
/* by sending 128B chunks to the OBC_PACKET_ROUTER, which in turn will	*/
/* attempt to downlink the telemetry to ground.							*/
/************************************************************************/
static void send_hk_as_tm(void)
{
	uint8_t i;
	clear_current_command();
	current_command[146] = HK_REPORT;
	for(i = 0; i < DATA_LENGTH; i++)
	{
		current_command[i] = current_hk[i];
	}
	xQueueSendToBack(hk_to_obc_fifo, current_command, (TickType_t)1);
	return;
}

/************************************************************************/
/* REPORT_SCHEDULE														*/
/* @Purpose: This function will downlink the current hk definition being*/
/* used to produce housekeeping reports.								*/
/* It does this by sending current hk def to OBC_PACKET_ROUTER.			*/
/************************************************************************/
static void send_param_report(void)
{
	uint8_t i;
	param_report_requiredf = 0;
	clear_current_command();
	current_command[146] = HK_DEFINITON_REPORT;
	for(i = 0; i < DATA_LENGTH; i++)
	{
		current_command[i] = current_hk_definition[i];
	}
	xQueueSendToBack(hk_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY if this doesn't return pdPASS
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
	current_command[144] = HK_TASK_ID;			// APID of this task
	current_command[140] = ((uint8_t)packet_id) >> 8;
	current_command[139] = (uint8_t)packet_id;
	current_command[138] = ((uint8_t)psc) >> 8;
	current_command[137] = (uint8_t)psc;
	xQueueSendToBackTask(HK_TASK_ID, 1, hk_to_obc_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY if this doesn't return pdPASS
	return;
}

// This function will kill the housekeeping task.
// If it is being called by the hk task 0 is passed, otherwise it is probably the FDIR task and 1 should be passed.
void housekeep_kill(uint8_t killer)
{
	// Kill the task.
	if(killer)
		vTaskDelete(housekeeping_HANDLE);
	else
		vTaskDelete(NULL);	
	return;
}
