/*
	Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		stk600_test0.c
	*
	*	PURPOSE:		
	*	This file was created to test a basic level of communication between the ATSAM3X8E
	*	on the Arduino DUE and the ATMEGA32M1 on an STK600 development board. The goal is to send
	*	messages between the two micro controllers over CAN and have them respond by blinking LEDs.
	*
	*	FILE REFERENCES:	stdio.h, FreeRTOS.h, task.h, partest.h, asf.h, can_func.h
	*
	*	EXTERNAL VARIABLES:		None:
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
	*	01/24/2015		Created.
	*
	*	DESCRIPTION:	
	*
	*	This file is being used to house test communication with the STK600.
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

void stk600_test0( void );

void stk600_test0(void)
{
	while(1)
	{
	pio_toggle_pin(LED0_GPIO);
	can_reset_all_mailbox(CAN0);
	can_reset_all_mailbox(CAN1);

	/* Init CAN0 Mailbox 0 to Reception Mailbox. */
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 0;
	can0_mailbox.uc_obj_type = CAN_MB_RX_MODE;
	can0_mailbox.ul_id_msk = CAN_MAM_MIDvA_Msk | CAN_MAM_MIDvB_Msk;
	can0_mailbox.ul_id = NODE0_ID;
	can_mailbox_init(CAN0, &can0_mailbox);

	/* Init CAN1 Mailbox 0 to Transmit Mailbox. */
	reset_mailbox_conf(&can1_mailbox);
	can1_mailbox.ul_mb_idx = 0;
	can1_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can1_mailbox.uc_tx_prio = 15;
	can1_mailbox.uc_id_ver = 0;		// Standard Frame
	can1_mailbox.ul_id_msk = 0;
	can_mailbox_init(CAN1, &can1_mailbox);

	/* Write transmit information into mailbox. */
	can1_mailbox.ul_id = NODE0_ID;
	can1_mailbox.ul_datal = COMMAND_IN;
	can1_mailbox.ul_datah = CAN_MSG_DUMMY_DATA;
	can1_mailbox.uc_length = MAX_CAN_FRAME_DATA_LEN;
	can_mailbox_write(CAN1, &can1_mailbox);

	/* Enable CAN0 mailbox 0 interrupt. */
	can_enable_interrupt(CAN0, CAN_IER_MB0);

	/* Send out the information in the mailbox. */
	can_global_send_transfer_cmd(CAN1, CAN_TCR_MB0);

	}

	while (1) { }
	
}

