/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		can_func.c
	*
	*	PURPOSE:		
	*	This file contains routines and interrupt handlers that pertain to the
	*	Controller Area Network (CAN).
	*
	*	FILE REFERENCES:	can_func.h
	*
	*	EXTERNAL VARIABLES:		g_ul_recv_status  (= 1 when a message was received)
	*
	*	EXTERNAL REFERENCES:	Same a File References.
	*
	*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
	*
	*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
	*
	*	NOTES:	 READ THE COMMENTS WITHIN SEND_COMMAND()!!!
	*	
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:			
	*	New tasks should be written to use as much of CMSIS as possible. The ASF and 
	*	FreeRTOS API libraries should also be used whenever possible to make the program
	*	more portable.

	*	DEVELOPMENT HISTORY:		
	*	01/02/2015		Created.
	*
	*	02/01/2015		The CAN handlers have been edited such that they save and restore 
	*					their respective can objects before reading them. (Could this create
	*					an infinite loop?)
	*
	*					I have finished debugging a CAN API function that I have written called
	*					send_command(low, high, ID, PRIORITY), which will allow future developers
	*					to write code involving CAN much more easily.
	*
	*	02/17/2015		I have changed both send_command() and can_init() due to issues I encountered
	*					with setting the ID of can mailboxes and communicating with the STK600. What I
	*					found was that it is important to use CAN_MID_MIDvA(ID) wherever you set an ID
	*					because this is the format that was used to set the STANDARD ID TAG.
	*
	*	DESCRIPTION:	
	*
	*	This file is being used to house the housekeeping task.
	*	
 */

#include "can_func.h"

volatile uint32_t g_ul_recv_status = 0;

/**
 * \brief Default interrupt handler for CAN 1.
 */
void CAN1_Handler(void)
{
	uint32_t ul_status;
	/* Save the state of the can1_mailbox object */	
	save_can_object(&can1_mailbox, &temp_mailbox_C1);	//Doesn't erase the CAN message.
	
	ul_status = can_get_status(CAN1);
	if (ul_status & GLOBAL_MAILBOX_MASK) {
		for (uint8_t i = 0; i < CANMB_NUMBER; i++) {
			ul_status = can_mailbox_get_status(CAN1, i);
			
			if ((ul_status & CAN_MSR_MRDY) == CAN_MSR_MRDY) {
				can1_mailbox.ul_mb_idx = i;
				can1_mailbox.ul_status = ul_status;
				can_mailbox_read(CAN1, &can1_mailbox);
				
				/* Decode CAN Message */
				decode_can_msg(&can1_mailbox, CAN1);
				/*assert(g_ul_recv_status); ***Implement assert here.*/
				
				/* Restore the can0 mailbox object */
				restore_can_object(&can1_mailbox, &temp_mailbox_C1);
				break;
			}
		}
	}
}
/************************************************************************/
/* Default Interrupt Handler for CAN0								    */
/************************************************************************/

/**
 * \brief Default interrupt handler for CAN0
 */
void CAN0_Handler(void)
{
	uint32_t ul_status;
	/* Save the state of the can0_mailbox object */
	save_can_object(&can0_mailbox, &temp_mailbox_C0);

	ul_status = can_get_status(CAN0);
	if (ul_status & GLOBAL_MAILBOX_MASK) {
		for (uint8_t i = 0; i < CANMB_NUMBER; i++) {
			ul_status = can_mailbox_get_status(CAN0, i);
			
			if ((ul_status & CAN_MSR_MRDY) == CAN_MSR_MRDY) {
				can0_mailbox.ul_mb_idx = i;
				can0_mailbox.ul_status = ul_status;
				can_mailbox_read(CAN0, &can0_mailbox);
				g_ul_recv_status = 1;
				
				// Decode CAN Message
				decode_can_msg(&can0_mailbox, CAN0);
				//assert(g_ul_recv_status); ***implement assert here.
				
				/* Restore the can0 mailbox object */
				restore_can_object(&can0_mailbox, &temp_mailbox_C0);
				break;
			}
		}
	}
}

/************************************************************************/
/* Decode CAN Message													*/
/* Performs a prescribed action depending on the message received       */
/************************************************************************/

/**
 * \brief Decodes the CAN message and performs a prescribed action depending on 
 * the message received.
 * @param *controller:  	CAN sending controller
 * @param *p_mailbox:		CAN mailbox structure of sending controller
 */
void decode_can_msg(can_mb_conf_t *p_mailbox, Can* controller)
{
	//assert(g_ul_recv_status);		// Only decode if a message was received.	***Asserts here.
	//assert(controller);				// CAN0 or CAN1 are nonzero.
	uint32_t ul_data_incom = p_mailbox->ul_datal;
	if(controller == CAN0)
		pio_toggle_pin(LED0_GPIO);
	if(controller == CAN1)
		pio_toggle_pin(LED1_GPIO);
	if (ul_data_incom == COMMAND_OUT)
		pio_toggle_pin(LED0_GPIO);
	if (ul_data_incom == COMMAND_IN)
		pio_toggle_pin(LED1_GPIO);
	if (ul_data_incom == DUMMY_COMMAND)
		pio_toggle_pin(LED1_GPIO);
	if (ul_data_incom == MSG_ACK)
		pio_toggle_pin(LED1_GPIO);

	if ((ul_data_incom == COMMAND_IN) & (controller == CAN0)) 
	{
		// Command has been received, respond.
		pio_toggle_pin(LED0_GPIO);
		command_in();
	}
	if ((ul_data_incom == COMMAND_OUT) & (controller == CAN1))
	{
		pio_toggle_pin(LED2_GPIO);	// LED2 indicates the response to the command
	}								// has been received.
	if ((ul_data_incom == HK_TRANSMIT) & (controller == CAN1))
	{
		pio_toggle_pin(LED3_GPIO);	// LED3 indicates housekeeping has been received.
	}
	if ((ul_data_incom == DUMMY_COMMAND) & (controller == CAN1))
	{
		pio_toggle_pin(LED3_GPIO);	// LED3 indicates housekeeping has been received.
	}
	
	if ((ul_data_incom == MSG_ACK) & (controller == CAN1))
	{
		pio_toggle_pin(LED3_GPIO);	// LED3 indicates the reception of a return message.
	}
	return;
}

/************************************************************************/
/* RESET MAILBOX CONFIGURE STRUCTURE                                    */
/************************************************************************/

/**
 * \brief Resets the mailbox configure structure.  
 * @param *p_mailbox:		Mailbox structure that will be reset. 
 */
void reset_mailbox_conf(can_mb_conf_t *p_mailbox)
{
	p_mailbox->ul_mb_idx = 0;
	p_mailbox->uc_obj_type = 0;
	p_mailbox->uc_id_ver = 0;
	p_mailbox->uc_length = 0;
	p_mailbox->uc_tx_prio = 0;
	p_mailbox->ul_status = 0;
	p_mailbox->ul_id_msk = 0;
	p_mailbox->ul_id = 0;
	p_mailbox->ul_fid = 0;
	p_mailbox->ul_datal = 0;
	p_mailbox->ul_datah = 0;
}

/************************************************************************/
/*                 SEND A 'COMMAND' FROM CAN1 TO CAN0		            */
/************************************************************************/

/**
 * \brief Sends a 'command' from CAN0 to CAN1
 */
void command_out(void)
{
	pio_toggle_pin(LED0_GPIO);
	can_reset_all_mailbox(CAN0);
	can_reset_all_mailbox(CAN1);

	/* Init CAN0 Mailbox 0 to Reception Mailbox. */
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 0;
	can0_mailbox.uc_obj_type = CAN_MB_RX_MODE;
	can0_mailbox.ul_id_msk = CAN_MAM_MIDvA_Msk | CAN_MAM_MIDvB_Msk;
	can0_mailbox.ul_id = CAN_MID_MIDvA(7);
	can_mailbox_init(CAN0, &can0_mailbox);

	/* Init CAN1 Mailbox 0 to Transmit Mailbox. */
	reset_mailbox_conf(&can1_mailbox);
	can1_mailbox.ul_mb_idx = 0;
	can1_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can1_mailbox.uc_tx_prio = 15;
	can1_mailbox.uc_id_ver = 0;
	can1_mailbox.ul_id_msk = 0;
	can_mailbox_init(CAN1, &can1_mailbox);

	/* Write transmit information into mailbox. */
	can1_mailbox.ul_id = CAN_MID_MIDvA(7);
	can1_mailbox.ul_datal = COMMAND_IN;
	can1_mailbox.ul_datah = CAN_MSG_DUMMY_DATA;
	can1_mailbox.uc_length = MAX_CAN_FRAME_DATA_LEN;
	can_mailbox_write(CAN1, &can1_mailbox);

	/* Enable CAN1 mailbox 0 interrupt. */
	can_enable_interrupt(CAN0, CAN_IER_MB0);

	/* Send out the information in the mailbox. */
	can_global_send_transfer_cmd(CAN1, CAN_TCR_MB0);

	/* potentially @non-terminating@ */
	while (!g_ul_recv_status) {
	}

}

/************************************************************************/
/*                 SEND A MESSAGE/COMMAND FROM CAN0				        */
/*	This function will take in two 32 bit integers representing the high*/
/*  bits and low bits of the message to be sent. It will then take in   */
/*  the ID of the message to be sent and it's priority, the message will*/
/*  then be sent out from CAN0 MB7.										*/
/*																		*/
/*  The function will return 1 if the action was completed and 0 if not.*/
/*	NOTE: a '1' does not indicate the transmission was successful.		*/
/*																		*/
/*	This function does not alter the can0_mailbox object.				*/
/************************************************************************/

uint32_t send_can_command(uint32_t low, uint32_t high, uint32_t ID, uint32_t PRIORITY)
{	
	/* We don't need to worry about saving and restoring the state of the can0_mailbox
	*  because the interrupt handlers should do that for us, and in the future only one
	*  task will be allowed in a CAN mailbox at any point in the through the use of
	*  mutex locks.
	*
	*  Another thing! Don't place this function in a for loop if you plan on using it to
	*  send from the same mailbox over and over again, make sure to yield in between calls
	*  if that is what you're trying to do. Also, be sure to release and acquire mutex locks
	*  in between each use of the CAN resource.
	*/
	
	
	/* Save current can0_mailbox object */
	can_temp_t temp_mailbox;
	save_can_object(&can0_mailbox, &temp_mailbox);
	
	/* Init CAN0 Mailbox 7 to Transmit Mailbox. */	
	/* CAN0 MB7 == COMMAND/MSG MB				*/
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 7;			//Mailbox Number 7
	can0_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can0_mailbox.uc_tx_prio = 9;		//Transmission Priority (Can be Changed dynamically)
	can0_mailbox.uc_id_ver = 0;
	can0_mailbox.ul_id_msk = 0;
	can_mailbox_init(CAN0, &can0_mailbox);

	/* Write transmit information into mailbox. */
	can0_mailbox.ul_id = CAN_MID_MIDvA(ID);			// ID of the message being sent,
	can0_mailbox.ul_datal = low;					// shifted over to the standard frame position.
	can0_mailbox.ul_datah = high;
	can0_mailbox.uc_length = MAX_CAN_FRAME_DATA_LEN;
	can_mailbox_write(CAN0, &can0_mailbox);

	/* Send out the information in the mailbox. */
	can_global_send_transfer_cmd(CAN0, CAN_TCR_MB7);
	
	/* Restore the can0_mailbox object */
	restore_can_object(&can0_mailbox, &temp_mailbox);
	
	return 1;
}


/************************************************************************/
/*				RESPOND TO THE COMMAND FROM CAN0 AND SEND TO CAN1       */
/************************************************************************/

/**
 * \brief Responds to he command from CAN0 and sends to CAN1
 **/
void command_in(void)
{
	pio_toggle_pin(LED0_GPIO);
	
	can_disable_interrupt(CAN0, CAN_IER_MB0);
	NVIC_DisableIRQ(CAN0_IRQn);
	
	can_reset_all_mailbox(CAN0);
	can_reset_all_mailbox(CAN1);

	/* Init CAN1 Mailbox 0 to Reception Mailbox. */
	reset_mailbox_conf(&can0_mailbox);
	can1_mailbox.ul_mb_idx = 1;
	can1_mailbox.uc_obj_type = CAN_MB_RX_MODE;
	can1_mailbox.ul_id_msk = CAN_MAM_MIDvA_Msk | CAN_MAM_MIDvB_Msk;
	can1_mailbox.ul_id = CAN_MID_MIDvA(7);
	can_mailbox_init(CAN1, &can1_mailbox);

	/* Init CAN0 Mailbox 0 to Transmit Mailbox. */
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 1;
	can0_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can0_mailbox.uc_tx_prio = 15;
	can0_mailbox.uc_id_ver = 0;
	can0_mailbox.ul_id_msk = 0;
	can_mailbox_init(CAN0, &can0_mailbox);

	/* Write transmit information into mailbox. */
	can0_mailbox.ul_id = CAN_MID_MIDvA(7);
	can0_mailbox.ul_datal = COMMAND_OUT;
	can0_mailbox.ul_datah = CAN_MSG_DUMMY_DATA;
	can0_mailbox.uc_length = MAX_CAN_FRAME_DATA_LEN;
	can_mailbox_write(CAN0, &can0_mailbox);

	/* Enable CAN1 mailbox 0 interrupt. */
	can_enable_interrupt(CAN1, CAN_IER_MB1);

	/* Send out the information in the mailbox. */
	can_global_send_transfer_cmd(CAN0, CAN_TCR_MB1);

	/* potentially @non-terminating@ */
	while (!g_ul_recv_status) {
	}
}

/************************************************************************/
/*					SAVING CAN OBJECTS                                  */
/*	This function will take in a mailbox object as the original pointer */
/*  and save each of it's elements in a temporary can structure.        */
/************************************************************************/

void save_can_object(can_mb_conf_t *original, can_temp_t *temp)
{
	/*This function takes in a mailbox object as the original pointer*/
	
	
	temp->ul_mb_idx		= original->ul_mb_idx;
	temp->uc_obj_type	= original->uc_obj_type;
	temp->uc_id_ver		= original->uc_id_ver;
	temp->uc_length		= original->uc_length;
	temp->uc_tx_prio	= original->uc_tx_prio;
	temp->ul_status		= original->ul_status;
	temp->ul_id_msk		= original->ul_id_msk;
	temp->ul_id			= original->ul_id;
	temp->ul_fid		= original->ul_fid;
	temp->ul_datal		= original->ul_datal;
	temp->ul_datah		= original->ul_datah;
	
	return;
}

/************************************************************************/
/*					RESTORING CAN OBJECTS                               */
/*	This function will take in a mailbox object as the original pointer */
/*  and restore each of it's elements from a temporary can structure.   */
/************************************************************************/

void restore_can_object(can_mb_conf_t *original, can_temp_t *temp)
{
	/*This function takes in a mailbox object as the original pointer*/	
	
	original->ul_mb_idx		= temp->ul_mb_idx; 
	original->uc_obj_type	= temp->uc_obj_type;
	original->uc_id_ver		= temp->uc_id_ver;
	original->uc_length		= temp->uc_length;
	original->uc_tx_prio	= temp->uc_tx_prio;
	original->ul_status		= temp->ul_status;
	original->ul_id_msk		= temp->ul_id_msk;
	original->ul_id			= temp->ul_id;
	original->ul_fid		= temp->ul_fid;
	original->ul_datal		= temp->ul_datal;
	original->ul_datah		= temp->ul_datah;
	
	return;
}

/**
 * \brief Initializes and enables CAN0 & CAN1 tranceivers and clocks. 
 * CAN0/CAN1 mailboxes are reset and interrupts disabled.
 */
void can_initialize(void)
{
	uint32_t ul_sysclk;
	uint32_t x = 1;

	/* Initialize CAN0 Transceiver. */
	sn65hvd234_set_rs(&can0_transceiver, PIN_CAN0_TR_RS_IDX);
	sn65hvd234_set_en(&can0_transceiver, PIN_CAN0_TR_EN_IDX);
	/* Enable CAN0 Transceiver. */
	sn65hvd234_disable_low_power(&can0_transceiver);
	sn65hvd234_enable(&can0_transceiver);

	/* Initialize CAN1 Transceiver. */
	sn65hvd234_set_rs(&can1_transceiver, PIN_CAN1_TR_RS_IDX);
	sn65hvd234_set_en(&can1_transceiver, PIN_CAN1_TR_EN_IDX);
	/* Enable CAN1 Transceiver. */
	sn65hvd234_disable_low_power(&can1_transceiver);
	sn65hvd234_enable(&can1_transceiver);

	/* Enable CAN0 & CAN1 clock. */
	pmc_enable_periph_clk(ID_CAN0);
	pmc_enable_periph_clk(ID_CAN1);

	ul_sysclk = sysclk_get_cpu_hz();
	if (can_init(CAN0, ul_sysclk, CAN_BPS_250K) &&
	can_init(CAN1, ul_sysclk, CAN_BPS_250K)) {

	/* Disable all CAN0 & CAN1 interrupts. */
	can_disable_interrupt(CAN0, CAN_DISABLE_ALL_INTERRUPT_MASK);
	can_disable_interrupt(CAN1, CAN_DISABLE_ALL_INTERRUPT_MASK);
		
	NVIC_EnableIRQ(CAN0_IRQn);
	NVIC_EnableIRQ(CAN1_IRQn);
	
	can_reset_all_mailbox(CAN0);
	can_reset_all_mailbox(CAN1);
	
	/* Initialize the CAN0 & CAN1 mailboxes */
	x = can_init_mailboxes(x); // Prevent Random PC jumps to this point.
	//configASSERT(x);
	
	
	}
	return;
}

uint32_t can_init_mailboxes(uint32_t x)
{
	/* Init CAN0 Mailbox 7 to Transmit Mailbox. */	
	/* CAN0 MB7 == COMMAND/MSG MB				*/
	//configASSERT(x);	//Check if this function was called naturally.
	
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 7;			//Mailbox Number 7
	can0_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can0_mailbox.uc_tx_prio = 5;		//Transmission Priority (Can be Changed dynamically)
	can0_mailbox.uc_id_ver = 0;
	can0_mailbox.ul_id_msk = 0;
	can_mailbox_init(CAN0, &can0_mailbox);
	
	/* Init CAN1 Mailbox 0 to Reception Mailbox. */
	reset_mailbox_conf(&can1_mailbox);
	can1_mailbox.ul_mb_idx = 0;
	can1_mailbox.uc_obj_type = CAN_MB_RX_MODE;
	can1_mailbox.ul_id_msk = CAN_MID_MIDvA_Msk | CAN_MID_MIDvB_Msk;	  // Compare the full 11 bits of the ID in both standard and extended.
	can1_mailbox.ul_id = CAN_MID_MIDvA(NODE0_ID);					  // The ID of CAN1 MB0 is currently NODE0_ID (standard).
	can_mailbox_init(CAN1, &can1_mailbox);
	
	can_enable_interrupt(CAN1, CAN_IER_MB0);
	
	return 1;
}
	
	