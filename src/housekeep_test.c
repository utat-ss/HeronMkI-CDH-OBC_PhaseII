/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		housekeep_test.c
	*
	*	PURPOSE:		
	*	This file will be used to initialize and test the housekeeping task.
	*
	*	FILE REFERENCES:	stdio.h, FreeRTOS.h, task.h, partest.h, asf.h, can_func.h
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
	*	Remember that configTICK_RATE_HZ in FreeRTOSConfig.h is currently set to 10 Hz and
	*	so when that is set to a new value, the amount of ticks in between housekeeping will
	*	have to be adjusted.
	*	
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:			
	*	New tasks should be written to use as much of CMSIS as possible. The ASF and 
	*	FreeRTOS API libraries should also be used whenever possible to make the program
	*	more portable.

	*	DEVELOPMENT HISTORY:		
	*	12/31/2014			Created
	*
	*	01/02/2015			Added CAN functionality and made use of vTaskDelayUntil(...) in order to
	*						implement a delay in between housekeeping requests.
	*
	*	DESCRIPTION:	
	*
	*	This file is being used to house the housekeeping task.
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

/* Priorities at which the tasks are created. */
#define HouseKeep_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )		// Lower the # means lower the priority

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define HK_PARAMETER			( 0xABCD )

/*-----------------------------------------------------------*/

/*
 * Functions Prototypes.
 */
static void prvHouseKeepTask( void *pvParameters );
void housekeep_test(void);

/************************************************************************/
/*			TEST FUNCTION FOR HOUSEKEEPING                              */
/************************************************************************/
/**
 * \brief Tests the housekeeping task.
 */
void housekeep_test( void )
{
		
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvHouseKeepTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) HK_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					HouseKeep_TASK_PRIORITY, 			/* The priority assigned to the task. */
					NULL );								/* The task handle is not required, so NULL is passed. */
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	/* @non-terminating@ */
}
/*-----------------------------------------------------------*/

/************************************************************************/
/*				HOUSEKEEPING TASK                                       */
/*			Comment as to what this does								*/
/************************************************************************/
/**
 * \brief Performs the housekeeping task.
 * @param *pvParameters:
 */
static void prvHouseKeepTask( void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == HK_PARAMETER );
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 15;	//Number entered here corresponds to the number of ticks we should wait.
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
	
	/* @non-terminating@ */
	for( ;; )
	{
			/* Init CAN0 Mailbox 3 to Producer Mailbox. */
			/* The subsystems should be doing this part */
			reset_mailbox_conf(&can0_mailbox);
			can0_mailbox.ul_mb_idx = 3;				// Mailbox 3
			can0_mailbox.uc_obj_type = CAN_MB_PRODUCER_MODE;
			can0_mailbox.ul_id_msk = 0;
			can0_mailbox.ul_id = CAN_MID_MIDvA(NODE0_ID);
			can_mailbox_init(CAN0, &can0_mailbox);

			/* Prepare the response information when subsystem receives a remote frame. */
			can0_mailbox.ul_datal = HK_TRANSMIT;
			can0_mailbox.ul_datah = CAN_MSG_DUMMY_DATA;
			can0_mailbox.uc_length = MAX_CAN_FRAME_DATA_LEN;
			can_mailbox_write(CAN0, &can0_mailbox);

			can_global_send_transfer_cmd(CAN0, CAN_TCR_MB3);

			/* Init CAN1 Mailbox 3 to Consumer Mailbox. It sends a remote frame and waits for an answer. */
			/* Here we will ask for HK from each subsystem sequentially, and wait for a response in between each */
			reset_mailbox_conf(&can1_mailbox);
			can1_mailbox.ul_mb_idx = 3;				// Mailbox 3
			can1_mailbox.uc_obj_type = CAN_MB_CONSUMER_MODE;
			can1_mailbox.uc_tx_prio = 9;
			can1_mailbox.ul_id_msk = CAN_MID_MIDvA_Msk | CAN_MID_MIDvB_Msk;
			can1_mailbox.ul_id = CAN_MID_MIDvA(NODE0_ID);
			can_mailbox_init(CAN1, &can1_mailbox);

			/* Enable CAN1 mailbox 3 interrupt. */
			can_enable_interrupt(CAN1, CAN_IER_MB3);

			can_global_send_transfer_cmd(CAN1, CAN_TCR_MB3);
			
			/* The remote request has been sent out and will be dealt with by the CAN_Handler */
			xLastWakeTime = xTaskGetTickCount();
		
			vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
	}
}
/*-----------------------------------------------------------*/

