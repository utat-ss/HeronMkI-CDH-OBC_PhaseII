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
	*	02/18/2015		I added the CAN API function request_houskeeping in order to use it in my second
	*					housekeeping test program. It takes the ID of the node in interest and requests
	*					housekeeping from it through the CAN0 MB6 (which is in consumer mode).
	*
	*					PHASE II.
	*
	*					I am removing the functions command_in() and command_out() because they are no 
	*					longer needed.
	*
	*	03/28/2015		I have added a global flag for when data has been received. In this way, the
	*					data-collection task will be able to see when a new value has been loaded into the
	*					CAN1_MB0 mailbox.
	*
	*					I added the API function can_message_read in order to be able to easily read from
	*					from can mailboxes when inside a task.
	*
	*	07/07/2015		A change which has been long time in coming is that instead of reading directly from
	*					the physical CAN mailbox objects, there will several types of global CAN registers
	*					which tasks will have access to through an API call.
	*
	*					Before accessing these registers, tasks will need to acquire the CAN mutex.
	*
	*					We shall have a different API call for each type of global CAN reg for the sake
	*					of readability on the part of the user.
	*
	*					I changed 'decode_can_msg' to 'debug_can_msg' and added the function 'stora_can_msg'
	*
	*					In the future we will get rid of the glob_drf and glob_comf flags as well as the 'stored'
	*					version of the glob vaiables because they are redundant and only useful for debugging.
	*
	*	07/12/2015		I have added an if statement in CAN1_Handler so that when the message: 0x0123456789ABCDEF
	*					is received, we are then allowed to exit safe mode.
	*
	*	DESCRIPTION:	
	*
	*					This file is being used for all functions and API related to all things CAN.	
 */

#include "can_func.h"

volatile uint32_t g_ul_recv_status = 0;

/************************************************************************/
/* Default Interrupt Handler for CAN1								    */
/************************************************************************/
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
				
				if((can1_mailbox.ul_datah == 0x01234567) && (can1_mailbox.ul_datal == 0x89ABCDEF))
				{
					SAFE_MODE = 0;
				}
				store_can_msg(&can1_mailbox, i);			// Save CAN Message to the appropriate global register.
				
				/* Decode CAN Message */
				debug_can_msg(&can1_mailbox, CAN1);
				/*assert(g_ul_recv_status); ***Implement assert here.*/
				
				/* Restore the can1 mailbox object */
				restore_can_object(&can1_mailbox, &temp_mailbox_C1);
				break;
			}
		}
	}
}
/************************************************************************/
/* Default Interrupt Handler for CAN0								    */
/************************************************************************/
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
				debug_can_msg(&can0_mailbox, CAN0);
				//assert(g_ul_recv_status); ***implement assert here.
				
				/* Restore the can0 mailbox object */
				restore_can_object(&can0_mailbox, &temp_mailbox_C0);
				break;
			}
		}
	}
}

/************************************************************************/
/* DEBUG CAN MESSAGE													*/
/* 																        */
/* Only for the purposes of debugging, this function blinks an LED 		*/
/* depending on the CAN message which was received.						*/
/*																		*/
/************************************************************************/

void debug_can_msg(can_mb_conf_t *p_mailbox, Can* controller)
{
	//assert(g_ul_recv_status);		// Only decode if a message was received.	***Asserts here.
	//assert(controller);				// CAN0 or CAN1 are nonzero.
	uint32_t ul_data_incom = p_mailbox->ul_datal;
	uint32_t uh_data_incom = p_mailbox->ul_datah;
	float temp;
	
	if ((ul_data_incom == MSG_ACK) & (controller == CAN1))
	{
		pio_toggle_pin(LED3_GPIO);	// LED3 indicates the reception of a return message.
	}
	
	if ((ul_data_incom == HK_RETURNED) & (controller == CAN1))
	{
		pio_toggle_pin(LED1_GPIO);	// LED1 indicates the reception of housekeeping.
	}
	
	if ((uh_data_incom == DATA_RETURNED) & (controller == CAN1) & (glob_drf == 0))
	{
		pio_toggle_pin(LED2_GPIO);	// LED2 indicates the reception of data.
		
		ul_data_incom = ul_data_incom >> 2;
		
		temp = (float)ul_data_incom;
		
		temp = temp * 0.03125;
		
		temp = temp;
		
		glob_drf = 1;
	}
	
	if ((uh_data_incom == MESSAGE_RETURNED) & (controller == CAN1) & (glob_comf == 0))
	{
		pio_toggle_pin(LED2_GPIO);	// LED2 indicates the reception of a message
		glob_comf = 1;
	}

	return;
}

/************************************************************************/
/* STORE CAN MESSAGE 				                                    */
/*																		*/
/* This function is used to take the CAN message which was received		*/
/* and store it in the appropriate global can register.					*/
/* 																		*/
/* These registers are then available to tasks through an API call.		*/
/************************************************************************/

void store_can_msg(can_mb_conf_t *p_mailbox, uint8_t mb)
{
	uint32_t ul_data_incom = p_mailbox->ul_datal;
	uint32_t uh_data_incom = p_mailbox->ul_datah;

	/* UPDATE THE GLOBAL CAN REGS		*/
	switch(mb)
	{		
	case 0 :
		can_glob_data_reg[0] = ul_data_incom;		// Global CAN Data Registers.
		can_glob_data_reg[1] = uh_data_incom;
	
	case 5 :
		can_glob_msg_reg[0] = ul_data_incom;		// Global CAN Message Registers.
		can_glob_msg_reg[1] = uh_data_incom;
	
	case 6 :
		can_glob_hk_reg[0] = ul_data_incom;			// Global CAN Housekeeping Resgiters.
		can_glob_hk_reg[1] = uh_data_incom;
	
	case 7 :
		can_glob_com_reg[0] = ul_data_incom;		// Global CAN Communications Registers.
		can_glob_com_reg[1] = uh_data_incom;

	default :
		return;
	}
	return;
}

/************************************************************************/
/* RESET MAILBOX CONFIGURE STRUCTURE                                    */
/************************************************************************/

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
	can0_mailbox.uc_tx_prio = PRIORITY;		//Transmission Priority (Can be Changed dynamically)
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
/*		READ CAN DATA 		                                            */
/*																		*/
/*		This function returns the CAN message currently residing in     */
/*		can_glob_data_reg.												*/
/*																		*/
/*		It does this by changing the value that the given pointers		*/
/* 		are pointing to.												*/
/*																		*/
/*		The function returns 1 is the operation was successful, 		*/
/*		otherwise it returns 0.
/************************************************************************/

uint32_t read_can_data(uint32_t* message_high, uint32_t* message_low, uint32_t access_code)
{
	// *** Implement an assert here on access_code.

	if (access_code == 1234)
	{
		*message_high = can_glob_data_reg[1];
		*message_low = can_glob_data_reg[0];
		return 1;
	}

	return 0;
}

/************************************************************************/
/*		READ CAN MESSAGE		                                           */
/*																		*/
/*		This function returns the CAN message currently residing in     */
/*		can_glob_data_msg.												*/
/*																		*/
/*		It does this by changing the value that the given pointers		*/
/* 		are pointing to.												*/
/*																		*/
/*		The function returns 1 is the operation was successful, 		*/
/*		otherwise it returns 0.
/************************************************************************/

uint32_t read_can_msg(uint32_t* message_high, uint32_t* message_low, uint32_t access_code)
{
	// *** Implement an assert here on access_code.

	if (access_code == 1234)
	{
		*message_high = can_glob_msg_reg[1];
		*message_low = can_glob_msg_reg[0];
		return 1;
	}

	return 0;
}

/************************************************************************/
/*		READ CAN HOUSEKEEPING 		                                    */
/*																		*/
/*		This function returns the CAN message currently residing in     */
/*		can_glob_data_reg.												*/
/*																		*/
/*		It does this by changing the value that the given pointers		*/
/* 		are pointing to.												*/
/*																		*/
/*		The function returns 1 is the operation was successful, 		*/
/*		otherwise it returns 0.
/************************************************************************/

uint32_t read_can_hk(uint32_t* message_high, uint32_t* message_low, uint32_t access_code)
{
	// *** Implement an assert here on access_code.

	if (access_code == 1234)
	{
		*message_high = can_glob_hk_reg[1];
		*message_low = can_glob_hk_reg[0];
		return 1;
	}

	return 0;
}

/************************************************************************/
/*		READ CAN COMMUNICATIONS PACKET	                                */
/*																		*/
/*		This function returns the CAN message currently residing in     */
/*		can_glob_data_reg.												*/
/*																		*/
/*		It does this by changing the value that the given pointers		*/
/* 		are pointing to.												*/
/*																		*/
/*		The function returns 1 is the operation was successful, 		*/
/*		otherwise it returns 0.
/************************************************************************/

uint32_t read_can_coms(uint32_t* message_high, uint32_t* message_low, uint32_t access_code)
{
	// *** Implement an assert here on access_code.

	if (access_code == 1234)
	{
		*message_high = can_glob_com_reg[1];
		*message_low = can_glob_com_reg[0];
		return 1;
	}

	return 0;
}

/************************************************************************/
/*                 REQUEST HOUSKEEPING TO BE SENT TO CAN0		        */
/*	This function will take in the ID of the node of interest. It will  */
/*	then send a housekeeping request to this node at a fixed priority	*/
/*	from CAN0 MB6.														*/
/*																		*/
/*  The function will return 1 if the action was completed and 0 if not.*/
/*	NOTE: a '1' does not indicate the transaction was successful.		*/
/*																		*/
/*	This function does not alter the can0_mailbox object.				*/
/************************************************************************/

uint32_t request_housekeeping(uint32_t ID)
{
	/* Save current can0_mailbox object */
	can_temp_t temp_mailbox;
	save_can_object(&can0_mailbox, &temp_mailbox);
	
	/* Init CAN0 Mailbox 6 to Housekeeping Request Mailbox. */	
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 6;			//Mailbox Number 6
	can0_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can0_mailbox.uc_tx_prio = HK_REQUEST_PRIO;		//Transmission Priority (Can be Changed dynamically)
	can0_mailbox.uc_id_ver = 0;
	can0_mailbox.ul_id_msk = 0;
	can_mailbox_init(CAN0, &can0_mailbox);

	/* Write transmit information into mailbox. */
	can0_mailbox.ul_id = CAN_MID_MIDvA(ID);			// ID of the message being sent,
	can0_mailbox.ul_datal = HK_REQUEST;				// shifted over to the standard frame position.
	can0_mailbox.ul_datah = HK_REQUEST;
	can0_mailbox.uc_length = MAX_CAN_FRAME_DATA_LEN;
	can_mailbox_write(CAN0, &can0_mailbox);

	/* Send out the information in the mailbox. */
	can_global_send_transfer_cmd(CAN0, CAN_TCR_MB6);
	
	/* Restore the can0_mailbox object */
	restore_can_object(&can0_mailbox, &temp_mailbox);
		
	return 1;
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

 /************************************************************************/
/*					CAN INITIALIZE 			                             */
/*	Initialzies and enables CAN0 & CAN1 transceivers and clocks.	     */
/*	CAN0/CAN1 mailboxes are reset and interrupts are disabled.			 */
/*																		 */
/*************************************************************************/
void can_initialize(void)
{
	uint32_t ul_sysclk;
	uint32_t x = 1, i = 0;

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
			can_init(CAN1, ul_sysclk, CAN_BPS_250K)) 
	{
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
		
		/* Initialize the data reception flag	*/
		glob_drf = 0;
		
		/* Initialize the message reception flag */
		glob_comf = 0;
		
		/* Initialize the global can regs		*/
		for (i = 0; i < 8; i++)
		{
			can_glob_com_reg[i] = 0;
			can_glob_data_reg[i] = 0;
			can_glob_hk_reg[i] = 0;
			glob_stored_data[i] = 0;
			glob_stored_message[i] = 0;
		}
	}
	return;
}

/************************************************************************/
/*					CAN INIT MAILBOXES                                  */
/*	This function initializes the different CAN mailbboxes.			    */
/* 																        */
/************************************************************************/

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
	
	/* Init CAN1 Mailbox 0 to Data Reception Mailbox. */
	reset_mailbox_conf(&can1_mailbox);
	can1_mailbox.ul_mb_idx = 0;				// Mailbox 0
	can1_mailbox.uc_obj_type = CAN_MB_RX_MODE;
	can1_mailbox.ul_id_msk = CAN_MID_MIDvA_Msk | CAN_MID_MIDvB_Msk;	  // Compare the full 11 bits of the ID in both standard and extended.
	can1_mailbox.ul_id = CAN_MID_MIDvA(CAN1_MB0);					  // The ID of CAN1 MB0 is currently CAN1_MB0 (standard).
	can_mailbox_init(CAN1, &can1_mailbox);
	
	/* Init CAN1 Mailbox 5 to Message Reception Mailbox. */
	reset_mailbox_conf(&can1_mailbox);
	can1_mailbox.ul_mb_idx = 5;				// Mailbox 5
	can1_mailbox.uc_obj_type = CAN_MB_RX_MODE;
	can1_mailbox.ul_id_msk = CAN_MID_MIDvA_Msk | CAN_MID_MIDvB_Msk;	  // Compare the full 11 bits of the ID in both standard and extended.
	can1_mailbox.ul_id = CAN_MID_MIDvA(CAN1_MB5);					  // The ID of CAN1 MB0 is currently CAN1_MB0 (standard).
	can_mailbox_init(CAN1, &can1_mailbox);
	
	/* Init CAN1 Mailbox 6 to HK Reception Mailbox. */
	reset_mailbox_conf(&can1_mailbox);
	can1_mailbox.ul_mb_idx = 6;				// Mailbox 6
	can1_mailbox.uc_obj_type = CAN_MB_RX_MODE;
	can1_mailbox.ul_id_msk = CAN_MID_MIDvA_Msk | CAN_MID_MIDvB_Msk;	  // Compare the full 11 bits of the ID in both standard and extended.
	can1_mailbox.ul_id = CAN_MID_MIDvA(CAN1_MB6);					  // The ID of CAN1 MB6 is currently CAN1_MB6 (standard).
	can_mailbox_init(CAN1, &can1_mailbox);
		
	/* Init CAN1 Mailbox 7 to Command Reception Mailbox. */
	reset_mailbox_conf(&can1_mailbox);
	can1_mailbox.ul_mb_idx = 7;				// Mailbox 7
	can1_mailbox.uc_obj_type = CAN_MB_RX_MODE;
	can1_mailbox.ul_id_msk = CAN_MID_MIDvA_Msk | CAN_MID_MIDvB_Msk;	  // Compare the full 11 bits of the ID in both standard and extended.
	can1_mailbox.ul_id = CAN_MID_MIDvA(CAN1_MB7);					  // The ID of CAN1 MB7 is currently CAN1_MB7 (standard).
	can_mailbox_init(CAN1, &can1_mailbox);
	
	can_enable_interrupt(CAN1, CAN_IER_MB0);
	can_enable_interrupt(CAN1, CAN_IER_MB5);
	can_enable_interrupt(CAN1, CAN_IER_MB6);
	can_enable_interrupt(CAN1, CAN_IER_MB7);
	
	/* Init CAN0 Mailbox 6 to Housekeeping Request Mailbox. */	
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 6;			//Mailbox Number 6
	can0_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can0_mailbox.uc_tx_prio = HK_REQUEST_PRIO;		//Transmission Priority (Can be Changed dynamically)
	can0_mailbox.uc_id_ver = 0;
	can0_mailbox.ul_id_msk = 0;
	can_mailbox_init(CAN0, &can0_mailbox);

	return 1;
}
	
	