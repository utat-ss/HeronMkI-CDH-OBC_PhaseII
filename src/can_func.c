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
	*	07/26/2015		store_can_data(...) now makes use of FIFOs as opposed to a single global can register,
	*					the read functions also make use of the same FIFOs. In this way, we now have a software
	*					FIFO which can buffer the incoming bytes of data from the CAN handlers. Note that
	*					the FIFOs are really Queues implented with the use of FreeRTOS's API functions.
	*	08/01/2015		We're changing the format with which we do CAN communication in order to make it
	*					more organized. The new system makes the higher 4 bytes of a CAN message the identifiers for it.
	*					The identifier includes a "FROM-WHO" byte, a BIG-TYPE byte (whether it's data, housekeeping, COMS, or
	*					a time-check), a SMALL-TYPE (so that we can be more specific), and a time-stamp (just the current minute).
	*					The lower four bytes are then what remain for CAN communication.
	*
	*	08/02/2015		I changed request_housekeeping in order to reflect the aforementioned change as well as the new
	*					structure for SSM mailboxes which is stated here for convenience:
	*					MB0: Receives data requests.
	*					MB1: Receives commands.
	*					MB2: Receives housekeeping requests.
	*					MB3: Receives Time Checks
	*					MB4,5: Used for sending messages.
	*
	*	08/07/2015		I have been working to formalize the CAN messages that the SSM sends to the OBC and hence 
	*					the reception and decoding structure that we use in can_func.c needed a bit of reworking. 
	*					Flags used by data_collect.c are now set using the function alert_can_data(). Additionally, 
	*					decode_can_command() has been implemented in order to anticipate commands that will be sent 
	*					to the OBC and need to be acted upon. The most important ones will be responses to commands that
	*					the OBC issues (the responses themselves are commands). This includes but is not limited to reprogramming 
	*					the SSM via CAN which has yet to be implemented.
	*
	*	08/08/2015		I have decided that a slight modification needs to be made to the CAN structure which I've implemented up to this point.
	*					The change that I'm proposing to make is to add a 'TO' or 'destination' tag at the beginning of CAN messages.
	*					The reason this comes up is that a large portion of the commands are going to be responses to requests
	*					made by the OBC and it would be convenient to know exactly which task on the OBC the response
	*					is intended for.
	*					
	*					Added a field to high_command_generator().
	*
	*	08/09/2015		Finished adding API functions read_from_SSM() and write_from_SSM(), this functions will give a task
	*					the ability to read or write from any address within the data section of an SSM's memory.
	*					I modified decode_can_command() so that is sets the appropriate flags and global variables upon
	*					reception of a response to these commands. 
	*					NOTE: these new functions will wait a maximum of one second for the operation to be completed.
	*
	*	10/07/2015		I am adding a flag named "transmit_complete" to glob_var.h Using this, I am going to make the API functions
	*					wait for a certain period of time for the value to become 1 they will return a failure code.
	*					This flag will be set within CAN0_Handler and reset within the initiating function.
	*
	*					In addition I am going to be moving our use of mutexes into the APIs themselves so that users do
	*					not have to understand mutex placement.
	*
	*					I also took the time to edit all the headers for the functions to make them all proper.
	*
	*					I got rid of the "transmit_complete" flags as they were a terrible idea.
	*
	*	10/31/2015		I am adding a few API functions for sending and receiving PUS packets. PUS packets are 143 Bytes long
	*					and hence need a form of sequence control so that they can be sent/received in 4B chunks.
	*
	*					I'm making a slight modification to the high_command_generator function so that it takes in the
	*					ssm_id as well as the sender ID. Byte 7 = (sender_id << 4) & ssm_id;
	*
	*					I'm no longer using CURRENT_MINUTE in high_command_generator().
	*
	*					I am putting the functionality of high_command_generator() inside of send_can_command() so that
	*					users have one less API to worry about.
	*					
	*
	*	DESCRIPTION:	
	*
	*					This file is being used for all functions and API related to all things CAN.	
 */

#include "can_func.h"

volatile uint32_t g_ul_recv_status = 0;

/************************************************************************/
/* Interrupt Handler for CAN1								    		*/
/************************************************************************/
void CAN1_Handler(void)
{
	uint32_t ul_status;
	/* Save the state of the can1_mailbox object */	
   	save_can_object(&can1_mailbox, &temp_mailbox_C1);	//Doesn't erase the CAN message.
	
	ul_status = can_get_status(CAN1);
	if (ul_status & GLOBAL_MAILBOX_MASK) 
	{
		for (uint8_t i = 0; i < CANMB_NUMBER; i++) 
		{
			ul_status = can_mailbox_get_status(CAN1, i);
			
			if ((ul_status & CAN_MSR_MRDY) == CAN_MSR_MRDY) 
			{
				can1_mailbox.ul_mb_idx = i;
				can1_mailbox.ul_status = ul_status;
				can_mailbox_read(CAN1, &can1_mailbox);
				
				if((can1_mailbox.ul_datah == 0x01234567) && (can1_mailbox.ul_datal == 0x89ABCDEF))
				{
					SAFE_MODE = 0;
				}
				store_can_msg(&can1_mailbox, i);			// Save CAN Message to the appropriate global register.
				
				/* Debug CAN Message 	*/
				debug_can_msg(&can1_mailbox, CAN1);
				/* Decode CAN Message 	*/
				if (i == 7)
					decode_can_command(&can1_mailbox, CAN1);

				if (i == 0)
					alert_can_data(&can1_mailbox, CAN1);
				/*assert(g_ul_recv_status); ***Implement assert here.*/
				
				/* Restore the can1 mailbox object */
				restore_can_object(&can1_mailbox, &temp_mailbox_C1);
				break;
			}
		}
	}
}
/************************************************************************/
/* Interrupt Handler for CAN0										    */
/************************************************************************/
void CAN0_Handler(void)
{
	uint32_t ul_status;

	ul_status = can_get_status(CAN0);
	if (ul_status & GLOBAL_MAILBOX_MASK) 
	{
		for (uint8_t i = 0; i < CANMB_NUMBER; i++) 
		{
			ul_status = can_mailbox_get_status(CAN0, i);
			
			if ((ul_status & CAN_MSR_MRDY) == CAN_MSR_MRDY) 
			{
				//assert(g_ul_recv_status); ***implement assert here.
				break;
			}
		}
	}
}

/************************************************************************/
/* DEBUG CAN MESSAGE 													*/
/* USED FOR debugging 													*/
/************************************************************************/
void debug_can_msg(can_mb_conf_t *p_mailbox, Can* controller)
{
	uint32_t ul_data_incom = p_mailbox->ul_datal;
	uint32_t uh_data_incom = p_mailbox->ul_datah;
	uint8_t big_type, small_type;

	big_type = (uint8_t)((uh_data_incom & 0x00FF0000)>>16);
	small_type = (uint8_t)((uh_data_incom & 0x0000FF00)>>8);

	if ((big_type == MT_COM) && (small_type == RESPONSE))
		pio_toggle_pin(LED3_GPIO);	// LED2 indicates a command response.

	if (big_type == MT_HK)
		pio_toggle_pin(LED1_GPIO);	// LED1 indicates the reception of housekeeping.
	
	if (big_type == MT_DATA)
		pio_toggle_pin(LED2_GPIO);	// LED2 indicates the reception of data.

	return;
}

/************************************************************************/
/* DECODE_CAN_COMMAND 		                                            */
/* @param: *p_mailbox: The mailbox object which we would like to decode */
/* CAN commands from.													*/
/* @param: *controller: used to verify authenticity						*/
/* @Purpose: This function decodes commands which are received and 		*/
/* performs different actions based on what was received. 				*/
/************************************************************************/
void decode_can_command(can_mb_conf_t *p_mailbox, Can* controller)
{
	//assert(g_ul_recv_status);		// Only decode if a message was received.	***Asserts here.
	//assert(controller);				// CAN0 or CAN1 are nonzero.
	uint32_t ul_data_incom = p_mailbox->ul_datal;
	uint32_t uh_data_incom = p_mailbox->ul_datah;
	uint8_t sender, destination, big_type, small_type;
	BaseType_t wake_task;	// Not needed here.

	sender = (uint8_t)(uh_data_incom >> 28);
	destination = (uint8_t)((uh_data_incom & 0x0F000000)>>24);
	big_type = (uint8_t)((uh_data_incom & 0x00FF0000)>>16);
	small_type = (uint8_t)((uh_data_incom & 0x0000FF00)>>8);

	if(big_type != MT_COM)
		return;
	
	switch(small_type)	// FROM WHO
	{
		case ACK_READ:
			switch(destination)
			{
				case HK_TASK_ID :
					if(hk_read_requestedf)
					{
						hk_read_receivedf = 1;
						hk_read_receive[1] = uh_data_incom;
						hk_read_receive[0] = ul_data_incom;
					}
					break;
				default :
					break;
			}
			break;
		case ACK_WRITE :
			switch(destination)
			{
				case HK_TASK_ID :
					if(hk_write_requestedf)
					{
						hk_write_receivedf = 1;
						hk_write_receive[1] = uh_data_incom;
						hk_write_receive[0] = ul_data_incom;
					}
					break;
				default :
					break;
			}
			break;
		case SEND_TC:
			xQueueSendToBackFromISR(tc_msg_fifo, &ul_data_incom, &wake_task);		// Telecommand reception FIFO.
			xQueueSendToBackFromISR(tc_msg_fifo, &uh_data_incom, &wake_task);
		case TC_PACKET_READY:
			start_tc_packet();
		case TM_TRANSACTION_RESP:
			tm_transfer_completef = (uint8_t)(ul_data_incom & 0x000000FF);
			break;
		case OK_START_TM_PACKET:
			start_tm_transferf = 1;
			break;
		default :
			break;
	}
	return;
}

/************************************************************************/
/* ALERT_CAN_DATA 	 		                                            */
/* @param: *p_mailbox: The mailbox object which we would like to use to */
/* create CAN alerts. 													*/
/* @param: *controller: To be used to verify that the request was genuin*/
/* @Purpose: This function sets flags which let process know that they 	*/
/* have data waiting for them. 											*/
/************************************************************************/
void alert_can_data(can_mb_conf_t *p_mailbox, Can* controller)
{
	//assert(g_ul_recv_status);		// Only decode if a message was received.	***Asserts here.
	//assert(controller);				// CAN0 or CAN1 are nonzero.
	uint32_t uh_data_incom = p_mailbox->ul_datah;
	uint32_t ul_data_incom = p_mailbox->ul_datal;
	uint8_t big_type, small_type, destination;

	big_type = (uint8_t)((uh_data_incom & 0x00FF0000)>>16);
	small_type = (uint8_t)((uh_data_incom & 0x0000FF00)>>8);
	destination = (uint8_t)((uh_data_incom & 0x0F000000)>>24);

	if(big_type != MT_DATA)
		return;
	
	if(small_type == SPI_TEMP1)
		glob_drf = 1;
		
	if(small_type == COMS_PACKET)
		glob_comsf = 1;
		
	switch(destination)
	{
		case EPS_TASK_ID:
			eps_data_receivedf = 1;
			eps_data_receive[1] = uh_data_incom;
			eps_data_receive[0] = ul_data_incom;
		case COMS_TASK_ID:
			coms_data_receivedf = 1;
			coms_data_receive[1] = uh_data_incom;
			coms_data_receive[0] = ul_data_incom;
		case PAY_TASK_ID:
			pay_data_receivedf = 1;
			pay_data_receive[1] = uh_data_incom;
			pay_data_receive[0] = ul_data_incom;
		default:
			return;	
	}
	return;
}

/************************************************************************/
/* STORE_CAN_MESSAGE 		                                            */
/* @param: *p_mailbox: The mailbox structure which we would like to 	*/
/* store in memory.														*/
/* @param: mb: The mailbox from which the message was received. 		*/
/* @Purpose: This function takes a message which was received and stores*/
/* in the proper FIFO in memory.										*/
/************************************************************************/
void store_can_msg(can_mb_conf_t *p_mailbox, uint8_t mb)
{
	uint32_t ul_data_incom = p_mailbox->ul_datal;
	uint32_t uh_data_incom = p_mailbox->ul_datah;
	BaseType_t wake_task;	// Not needed here.

	/* UPDATE THE GLOBAL CAN REGS		*/
	switch(mb)
	{		
	case 0 :
		xQueueSendToBackFromISR(can_data_fifo, &ul_data_incom, &wake_task);		// Global CAN Data FIFO
		xQueueSendToBackFromISR(can_data_fifo, &uh_data_incom, &wake_task);
		
	case 5 :
		xQueueSendToBackFromISR(can_msg_fifo, &ul_data_incom, &wake_task);		// Global CAN Message FIFO
		xQueueSendToBackFromISR(can_msg_fifo, &uh_data_incom, &wake_task);
	
	case 6 :
		xQueueSendToBackFromISR(can_hk_fifo, &ul_data_incom, &wake_task);		// Global CAN HK FIFO.
		xQueueSendToBackFromISR(can_hk_fifo, &uh_data_incom, &wake_task);
	
	case 7 :
		xQueueSendToBackFromISR(can_com_fifo, &ul_data_incom, &wake_task);		// Global CAN Command FIFO
		xQueueSendToBackFromISR(can_com_fifo, &uh_data_incom, &wake_task);
		// Note that the above line is not going to be necessary in the future
		// as we shall use decode_can_msg() to set flags which processes will then
		// be able to use without reading CAN messages.
		// Of course, CAN messages and FIFOs will still be used to transmit info
		// to the requesting process.

	default :
		return;
	}
	return;
}

/************************************************************************/
/* RESET_MAILBOX_CONF 		                                            */
/* @param: *p_mailbox: the mailbox object to be reset. 					*/
/* @Purpose: This function resets the attributes of object p_mailbox.	*/
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

	return;
}

/************************************************************************/
/* SEND_CAN_COMMAND_H 		                                            */
/* @param: low: The lower 4 bytes that you would like to send.			*/
/* @param: high: The upper 4 bytes that you would like to send.			*/
/* @param: ID: The ID of the SSM that you are sending a command to.		*/
/* @param: PRIORITY: The priority which will be put on the CAN message. */
/* @Purpose: This function sends an 8 byte message to the SSM chosen. 	*/
/* @return: 1 == completed, 0 == failure.								*/
/* @NOTE: 1 != Success (Necessarily) 									*/
/* @NOTE: This is a helper function, it is only to be used in sections  */
/* of code where the Can0_Mutex has been acquired.						*/
/************************************************************************/
uint32_t send_can_command_h(uint32_t low, uint32_t high, uint32_t ID, uint32_t PRIORITY)
{	
	uint32_t timeout = 8400;		// ~ 100 us timeout.
	/* Init CAN0 Mailbox 7 to Transmit Mailbox. */	
	/* CAN0 MB7 == COMMAND/MSG MB				*/
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 7;						//Mailbox Number 7
	can0_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can0_mailbox.uc_tx_prio = PRIORITY;				//Transmission Priority (Can be Changed dynamically)
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
	
	return 0;
}

/************************************************************************/
/* SEND_CAN_COMMAND 		                                            */
/* @param: low: The lower 4 bytes that you would like to send.			*/
/* @param: high: The upper 4 bytes that you would like to send.			*/
/* @param: ID: The ID of the SSM that you are sending a command to.		*/
/* @param: PRIORITY: The priority which will be put on the CAN message. */
/* @Purpose: This function sends an 8 byte message to the SSM chosen. 	*/
/* @return: 1 == completed, (<=0) == failure.							*/
/* @NOTE: 1 != Success (Necessarily) 									*/
/************************************************************************/
int send_can_command(uint32_t low, uint8_t byte_four, uint8_t sender_id, uint8_t ssm_id, uint8_t smalltype, uint8_t priority)
{	
	uint32_t timeout = 8400;		// ~ 100 us timeout.
	uint32_t id, ret_val, high;
	
	if(ssm_id == COMS_ID)
		id = SUB0_ID0;
	if(ssm_id == EPS_ID)
		id = SUB1_ID0;
	if(ssm_id == PAY_ID)
		id = SUB2_ID0;
		
	high = high_command_generator(sender_id, ssm_id, MT_COM, smalltype);
	if(byte_four)
		high &= (uint32_t)byte_four;

	if (xSemaphoreTake(Can0_Mutex, (TickType_t) 1) == pdTRUE)		// Attempt to acquire CAN1 Mutex, block for 1 tick.
	{
		ret_val = send_can_command_h(low, high, id, priority);
		xSemaphoreGive(Can0_Mutex);
		return (int)ret_val;
	}
	
	else
		return -1;												// CAN0 is currently busy, or something has gone wrong.
}

/************************************************************************/
/* READ_CAN_DATA 		 	                                            */
/*																		*/
/* @param: message_high: The upper 4 bytes of the message read.			*/
/* @param: message_low: The lower 4 bytes of the message read.			*/
/* @param: Access_code: Used to ensure that the request was genuine. 	*/
/* @Purpose: This function returns a CAN message curerntly residing in 	*/
/* the can_data_fifo queue. 											*/
/* @return: 1 == successful, 0 == failure.								*/
/************************************************************************/
uint32_t read_can_data(uint32_t* message_high, uint32_t* message_low, uint32_t access_code)
{
	// *** Implement an assert here on access_code.
	if (access_code == 1234)
	{
		if(xQueueReceive(can_data_fifo, message_low, (TickType_t) 1) == pdTRUE)
		{
			if(xQueueReceive(can_data_fifo, message_high, (TickType_t) 1) == pdTRUE)
			{
				return 1;
			}
			else
				return -1;
		}
		else
			return -1;
	}
	return -1;
}

/************************************************************************/
/* READ_CAN_MSG 		 	                                            */
/*																		*/
/* @param: message_high: The upper 4 bytes of the message read.			*/
/* @param: message_low: The lower 4 bytes of the message read.			*/
/* @param: Access_code: Used to ensure that the request was genuine. 	*/
/* @Purpose: This function returns a CAN message curerntly residing in 	*/
/* the can_msg_fifo queue. 												*/
/* @return: 1 == successful, 0 == failure.								*/
/************************************************************************/
uint32_t read_can_msg(uint32_t* message_high, uint32_t* message_low, uint32_t access_code)
{
	// *** Implement an assert here on access_code.
	if (access_code == 1234)
	{
		if(xQueueReceive(can_msg_fifo, message_low, (TickType_t) 1) == pdTRUE)
		{
			if(xQueueReceive(can_msg_fifo, message_high, (TickType_t) 1) == pdTRUE)
			{
				return 1;
			}
			else
				return -1;
		}
		else
			return -1;
	}
	return -1;
}

/************************************************************************/
/* READ_CAN_HK  		 	                                            */
/*																		*/
/* @param: message_high: The upper 4 bytes of the message read.			*/
/* @param: message_low: The lower 4 bytes of the message read.			*/
/* @param: Access_code: Used to ensure that the request was genuine. 	*/
/* @Purpose: This function returns a CAN message curerntly residing in 	*/
/* the can_hk_fifo queue. 												*/
/* @return: 1 == successful, 0 == failure.								*/
/* @Note: This function will block for a maximum of 2 ticks.			*/
/************************************************************************/
uint32_t read_can_hk(uint32_t* message_high, uint32_t* message_low, uint32_t access_code)
{
	// *** Implement an assert here on access_code.
	if (access_code == 1234)
	{
		if(xQueueReceive(can_hk_fifo, message_low, (TickType_t) 1) == pdTRUE)
		{
			if(xQueueReceive(can_hk_fifo, message_high, (TickType_t) 1) == pdTRUE)
			{
				return 1;
			}
			else
			return -1;
		}
		else
		return -1;
	}
	return -1;
}

/************************************************************************/
/* READ_CAN_COMS 		 	                                            */
/*																		*/
/* @param: message_high: The upper 4 bytes of the message read.			*/
/* @param: message_low: The lower 4 bytes of the message read.			*/
/* @param: Access_code: Used to ensure that the request was genuine. 	*/
/* @Purpose: This function returns a CAN message curerntly residing in 	*/
/* the can_com_fifo queue. 												*/
/* @return: 1 == successful, 0 == failure.								*/
/************************************************************************/
uint32_t read_can_coms(uint32_t* message_high, uint32_t* message_low, uint32_t access_code)
{
	// *** Implement an assert here on access_code.
	if (access_code == 1234)
	{
		if(xQueueReceive(can_com_fifo, message_low, (TickType_t) 1) == pdTRUE)
		{
			if(xQueueReceive(can_com_fifo, message_high, (TickType_t) 1) == pdTRUE)
			{
				return 1;
			}
			else
			return -1;
		}
		else
		return -1;
	}
	return -1;
}

/************************************************************************/
/* HIGH_COMMAND_GENERATOR 	                                            */
/*																		*/
/* @param: ID: The ID of the SSM which you would like to get hk from.	*/
/* @Purpose: This function will take in the ID of the node of interest	*/
/* It will then send a houeskeeping request to this node at a fixed 	*/
/* priority from CAN0 MB6.												*/
/* @return: 1 == complete, 0 == incomplete.								*/
/* @NOTE: 1 != successful (necessarily)									*/
/************************************************************************/
uint32_t request_housekeeping(uint32_t ssm_id)
{
	uint32_t high, id;
	uint32_t timeout = 8400;		// ~ 100 us timeout.
	
	if(ssm_id == COMS_ID)
		id = SUB0_ID5;				// Housekeeping request mailbox in the SSM.
	if(ssm_id == EPS_ID)
		id = SUB1_ID5;
	if(ssm_id == PAY_ID)
		id = SUB2_ID5;

	if (xSemaphoreTake(Can0_Mutex, (TickType_t) 1) == pdTRUE)		// Attempt to acquire CAN1 Mutex, block for 1 tick.
	{
		/* Init CAN0 Mailbox 6 to Housekeeping Request Mailbox. */	
		reset_mailbox_conf(&can0_mailbox);
		can0_mailbox.ul_mb_idx = 6;			//Mailbox Number 6
		can0_mailbox.uc_obj_type = CAN_MB_TX_MODE;
		can0_mailbox.uc_tx_prio = 10;		//Transmission Priority (Can be Changed dynamically)
		can0_mailbox.uc_id_ver = 0;
		can0_mailbox.ul_id_msk = 0;
		can_mailbox_init(CAN0, &can0_mailbox);

		high = high_command_generator(HK_TASK_ID, ssm_id, MT_COM, REQ_HK);

		/* Write transmit information into mailbox. */
		can0_mailbox.ul_id = CAN_MID_MIDvA(id);			// ID of the message being sent,
		can0_mailbox.ul_datal = 0x00;				// shifted over to the standard frame position.
		can0_mailbox.ul_datah = high;
		can0_mailbox.uc_length = MAX_CAN_FRAME_DATA_LEN;
		can_mailbox_write(CAN0, &can0_mailbox);

		/* Send out the information in the mailbox. */
		can_global_send_transfer_cmd(CAN0, CAN_TCR_MB6);		
		xSemaphoreGive(Can0_Mutex);
		delay_us(100);
		return 1;
	}
	else
		return -1;										// CAN0 is currently busy, or something has gone wrong.
}

/************************************************************************/
/* SAVE_CAN_OBJECT 		 	                                            */
/*																		*/
/* @param: original: The original CAN object which we would like to save*/
/* @param: temp: The temporary CAN object that we are putting orig in.	*/
/* @param: smallType: Look at the list small_types in can_func.h, pick  */
/* the one that enables the functionality you want.						*/
/* @Purpose: The function takes all the attributes of the original		*/
/* object and stores them in the temp object.							*/
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
/* RESTORE_CAN_OBJECT 	 	                                            */
/*																		*/
/* @param: *original: A pointer to the original CAN object.				*/
/* @param: *temp: A pointer to what was the temporary CAN object.		*/
/* @Purpose: This function replaces all the attributes of the "original"*/
/* object with all the attributes in the "temp" object.					*/
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
/* CAN_INITIALIZE 			                                            */
/* @Purpose: Initializes and enables CAN0 & CAN1 controllers and clocks.*/
/* CAN0/CAN1 mailboxes are reset and interrupts are disabled.			*/
/************************************************************************/
void can_initialize(void)
{
	uint32_t ul_sysclk;
	uint32_t x = 1, i = 0;
	UBaseType_t fifo_length, item_size;

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

		NVIC_EnableIRQ(CAN1_IRQn);
		
		can_reset_all_mailbox(CAN0);
		can_reset_all_mailbox(CAN1);
		
		/* Initialize the CAN0 & CAN1 mailboxes */
		x = can_init_mailboxes(x); // Prevent Random PC jumps to this point.
		//configASSERT(x);
		
		/* Initialize the data reception flag	*/
		glob_drf = 0;
		
		/* Initialize the message reception flag */
		glob_comsf = 0;
		
		/* Initialize the HK Command Flags */
		hk_read_requestedf = 0;
		hk_read_receivedf = 0;
		hk_write_requestedf = 0;
		hk_write_receivedf = 0;
		
		/* Initialize the global can regs		*/
		for (i = 0; i < 2; i++)
		{
			glob_stored_data[i] = 0;
			glob_stored_message[i] = 0;
			hk_read_receive[i] = 0;
			hk_write_receive[i] = 0;
		}
		
		/* Initialize Global variables and buffers related to PUS packts */
		for (i = 0; i < 143; i++)
		{
			current_tc[i] = 0;
			current_tm[i] = 0;
			tc_to_decode[i] = 0;
		}
		tm_transfer_completef = 0;
		start_tm_transferf = 0;
		current_tc_fullf = 0;
		receiving_tcf = 0;
		
		/* Initialize global CAN FIFOs					*/
		fifo_length = 100;		// Max number of items in the FIFO.
		item_size = 4;			// Number of bytes in the items.
		
		/* This corresponds to 400 bytes, or 50 CAN messages */
		can_data_fifo = xQueueCreate(fifo_length, item_size);
		can_msg_fifo = xQueueCreate(fifo_length, item_size);
		can_hk_fifo = xQueueCreate(fifo_length, item_size);
		can_com_fifo = xQueueCreate(fifo_length, item_size);
		tc_msg_fifo = xQueueCreate(fifo_length, item_size);

		/* Initialize global PUS Packet FIFOs			*/
		fifo_length = 2;			// Max number of items in the FIFO.
		item_size = 128;			// Number of bytes in the items

		/* This corresponds to 256 bytes, or 2 HK Collections */
		hk_to_tm_fifo = xQueueCreate(fifo_length, item_size);

		/* MAKE SURE TO SEND LOW 4 BYTES FIRST, AND RECEIVE LOW 4 BYTES FIRST. */
	}
	return;
}

/************************************************************************/
/* CAN_INIT_MAILBOXES 		                                            */
/*																		*/
/* @param: x: simply meant to be to confirm that this function was 		*/
/* called naturally.													*/
/* @Purpose: This function initializes the CAN mailboxes for use.		*/
/************************************************************************/
uint32_t can_init_mailboxes(uint32_t x)
{
	//configASSERT(x);	//Check if this function was called naturally.

	/* Init CAN0 Mailbox 7 to Transmit Mailbox. */
	/* CAN0 MB7 == COMMAND/MSG MB				*/	
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 7;			//Mailbox Number 7
	can0_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can0_mailbox.uc_tx_prio = 10;		//Transmission Priority (Can be Changed dynamically)
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
	
	/* Init CAN0 Mailbox 6 to Housekeeping Request Mailbox. */	
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 6;			//Mailbox Number 6
	can0_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can0_mailbox.uc_tx_prio = HK_REQUEST_PRIO;		//Transmission Priority (Can be Changed dynamically)
	can0_mailbox.uc_id_ver = 0;
	can0_mailbox.ul_id_msk = 0;
	can_mailbox_init(CAN0, &can0_mailbox);

	can_enable_interrupt(CAN1, CAN_IER_MB0);
	can_enable_interrupt(CAN1, CAN_IER_MB5);
	can_enable_interrupt(CAN1, CAN_IER_MB6);
	can_enable_interrupt(CAN1, CAN_IER_MB7);
	
	return 1;
}

/************************************************************************/
/* HIGH_COMMAND_GENERATOR 	                                            */
/*																		*/
/* @param: sender_id:	FROM-WHO, ex: EPS_TASK_ID						*/
/* @param: MessageType: Either MT_DATA, MT_HK, MT_COM, MT_TC			*/
/* @param: smallType: Look at the list small_types in can_func.h, pick  */
/* the one that enables the functionality you want.						*/
/* @Purpose: This function is used to generate the upper 4 bytes of all */
/* CAN messages as per the new structure. 								*/
/************************************************************************/
uint32_t high_command_generator(uint8_t sender_id, uint8_t ssm_id, uint8_t MessageType, uint8_t smalltype)
{
	uint32_t sender, m_type, s_type, destination;
	
	sender = (uint32_t)sender_id;
	sender = sender << 28;
	
	destination = (uint32_t)ssm_id;
	ssm_id = ssm_id << 24;
		
	m_type = (uint32_t)MessageType;
	m_type = m_type << 16;
	
	s_type = (uint32_t)smalltype;
	s_type = s_type << 8;
	
	return sender + destination + m_type + s_type;
}

/************************************************************************/
/* READ_FROM_SSM 	                                                    */
/*																		*/
/* @param: sender_id:	FROM-WHO, ex: EPS_TASK_ID						*/
/* @param: ssm_id:	Which SSM you are communicating with. ex: SUB1_ID0	*/
/* @param: passkey: a unique identifier used to verify that your request*/
/* actually went through.												*/
/* @param: addr: The address within the SSM you would like to read 1B	*/
/* @Purpose: This function can be used to read a single byte of data 	*/
/* to any address within the range 0x00-0xff							*/
/* NOTE: This is for use with tasks and their corresponding SSMs only.	*/
/* NOTE: This function will wait for a maximum of 250ms. for the		*/
/* operation to complete.												*/
/************************************************************************/
uint8_t read_from_SSM(uint8_t sender_id, uint8_t ssm_id, uint8_t passkey, uint8_t addr)
{
	uint32_t high, low, timeout;
	uint8_t pk, ret_val, p, a, id;
	
	if(ssm_id == COMS_ID)
		id = SUB0_ID0;
	if(ssm_id == EPS_ID)
		id = SUB1_ID0;
	if(ssm_id == PAY_ID)
		id = SUB2_ID0;
	
	timeout = 8000000;		// Maximum wait time of 100 ms.
	
	high = high_command_generator(sender_id, ssm_id, MT_COM, REQ_READ);
	p = (uint32_t)passkey;
	p = p << 24;
	a = (uint32_t)addr;
	low = p + a;

	if (xSemaphoreTake(Can0_Mutex, (TickType_t) 0) == pdTRUE)		// Attempt to acquire CAN1 Mutex, block for 1 tick.
	{
		hk_read_requestedf = 1;

		if (send_can_command_h(low, high, id, DEF_PRIO) < 1)
		{
			xSemaphoreGive(Can0_Mutex);
			return -1;
		}
		
		while(!hk_read_receivedf)	// Wait for the response to come back.
		{
			if(!timeout--)
			{
				hk_read_requestedf = 0;
				xSemaphoreGive(Can0_Mutex);
				return passkey;		// The read operation failed.
			}
		}
		hk_read_requestedf = 0;
		
		pk = (uint8_t)(hk_read_receive[0] >> 24);
		
		if ((pk != passkey))
		{
			hk_read_receivedf = 0;
			xSemaphoreGive(Can0_Mutex);
			return passkey;			// The read operation failed.
		}
			
		ret_val = (uint8_t)(hk_read_receive[0] & 0x000000FF);
		
		hk_read_receivedf = 0;		// Zero this last to keep in sync.
		
		xSemaphoreGive(Can0_Mutex);
		delay_us(100);
		return ret_val;				// This is the result of a read operation.
	}
	
	else
		return -1;					// CAN0 is currently busy or something has gone wrong.
}

/************************************************************************/
/* WRITE_TO_SSM 	                                                    */
/*																		*/
/* @param: sender_id:	FROM-WHO, ex: EPS_TASK_ID						*/
/* @param: ssm_id:	Which SSM you are communicating with. ex: SUB1_ID0	*/
/* @param: passkey: a unique identifier used to verify that your request*/
/* actually went through.												*/
/* @param: addr: The address within the SSM you would like to write to 	*/
/* @param: data: The single byte that would like to write to addr 		*/
/* @Purpose: This function can be used to write a single byte of data 	*/
/* to any address within the range 0x00-0xff							*/
/* NOTE: This is for use with tasks and their corresponding SSMs only.	*/
/* NOTE: This function will wait for a maximum of 250ms. for the		*/
/* operation to complete.												*/
/************************************************************************/
uint8_t write_to_SSM(uint8_t sender_id, uint8_t ssm_id, uint8_t passkey, uint8_t addr, uint8_t data)
{
	uint32_t high, low, timeout, p, a, d;
	uint8_t pk, ret_val, id;
	timeout = 8000000;		// Maximum wait time of 100 ms.
	
	if(ssm_id == COMS_ID)
		id = SUB0_ID0;
	if(ssm_id == EPS_ID)
		id = SUB1_ID0;
	if(ssm_id == PAY_ID)
		id = SUB2_ID0;

	if (xSemaphoreTake(Can0_Mutex, (TickType_t) 0) == pdTRUE)		// Attempt to acquire CAN1 Mutex, block for 1 tick.
	{
		high = high_command_generator(sender_id, ssm_id, MT_COM, REQ_WRITE);
		p = (uint32_t)passkey;
		p = p << 24;
		a = (uint32_t)addr;
		a = a << 8;
		d = (uint32_t)data;
		low = p + a + d;
		
		hk_write_requestedf = 1;

		if(send_can_command_h(low, high, id, DEF_PRIO) < 1)
		{
			xSemaphoreGive(Can0_Mutex);
			return -1;
		}
		
		while(!hk_write_receivedf)	// Wait for the response to come back.
		{
			if(!timeout--)
			{
				hk_write_requestedf = 0;
				xSemaphoreGive(Can0_Mutex);
				return passkey;		// The write operation failed.
			}
		}
		hk_write_requestedf = 0;
		
		pk = (uint8_t)(hk_write_receive[0] >> 24);
		
		if ((pk != passkey))
		{
			hk_write_receivedf = 0;
			xSemaphoreGive(Can0_Mutex);
			return passkey;			// The write operation failed.
		}
		
		ret_val = (uint8_t)(hk_read_receive[0] & 0x000000FF);
		
		if(ret_val > 0)
		{
			xSemaphoreGive(Can0_Mutex);
			delay_us(100);
			return 1;				// The write operation succeeded.
		}
		else
		{
			xSemaphoreGive(Can0_Mutex);
			return passkey;			// The write operation failed.
		}
	}
	else
		return -1;					// CAN0 was busy or something has gone wrong.
}

/************************************************************************/
/* REQUEST_SENSOR_DATA_H                                                */
/*																		*/
/* @param: sender_id:	FROM-WHO, ex: EPS_TASK_ID						*/
/* @param: ssm_id:	Which SSM you are communicating with. ex: SUB1_ID0	*/
/* @Purpose: This function can be used to retrieve sensor data from an 	*/
/* SSM. 																*/
/* NOTE: This is for use with tasks and their corresponding SSMs only.	*/
/* NOTE: This function will wait for a maximum of 25ms. for the			*/
/* operation to complete.												*/
/* NOTE: This function should only be used in sections of code where the*/
/* Can0_Mutex was acquired.												*/
/************************************************************************/

static uint32_t request_sensor_data_h(uint8_t sender_id, uint8_t ssm_id, uint8_t sensor_name, uint8_t* status)
{
	uint32_t high, low, timeout, s, ret_val;
	timeout = 2000000;		// Maximum wait time of 25ms.
	uint8_t id;
	
	high = high_command_generator(sender_id, ssm_id, MT_COM, REQ_DATA);
	low = (uint32_t)sensor_name;
	low = low << 24;
	
	if(ssm_id == COMS_ID)
		id = SUB0_ID0;
	if(ssm_id == EPS_ID)
		id = SUB1_ID0;
	if(ssm_id == PAY_ID)
		id = SUB2_ID0;

	if (send_can_command_h(low, high, id, DEF_PRIO) < 1)
	{
		*status = -1;
		return -1;
	}

	if(sender_id == EPS_TASK_ID)
	{
		while(!eps_data_receivedf)	// Wait for the response to come back.
		{
			if(!timeout--)
			{
				*status = -1;
				return -1;			// The operation failed.
			}
		}
		s = (uint8_t)((eps_data_receive[1] & 0x0000FF00) >> 8);	// Name of the sensor
	
		if (s != sensor_name)
		{
			eps_data_receivedf = 0;
			*status = -1;
			return -1;			// The operation failed.
		}
	
		ret_val = eps_data_receive[0];	// 32-bit return value.
	
		eps_data_receivedf = 0;		// Zero this last to keep in sync.
	}

	if(sender_id == COMS_TASK_ID)
	{
		while(!coms_data_receivedf)	// Wait for the response to come back.
		{
			if(!timeout--)
			{
				*status = -1;
				return -1;			// The operation failed.
			}
		}
		s = (uint8_t)((coms_data_receive[1] & 0x0000FF00) >> 8);	// Name of the sensor
	
		if (s != sensor_name)
		{
			coms_data_receivedf = 0;
			*status = -1;
			return -1;			// The operation failed.
		}
	
		ret_val = coms_data_receive[0];	// 32-bit return value.
	
		coms_data_receivedf = 0;		// Zero this last to keep in sync.
	}

	if(sender_id == PAY_TASK_ID)
	{
		while(!pay_data_receivedf)	// Wait for the response to come back.
		{
			if(!timeout--)
			{
				*status = -1;
				return -1;			// The operation failed.
			}
		}
		s = (uint8_t)((pay_data_receive[1] & 0x0000FF00) >> 8);	// Name of the sensor
	
		if (s != sensor_name)
		{
			pay_data_receivedf = 0;
			*status = -1;
			return -1;			// The operation failed.
		}
	
		ret_val = pay_data_receive[0];	// 32-bit return value.
	
		pay_data_receivedf = 0;		// Zero this last to keep in sync.
	}

	*status = 1;				// The operation succeeded.
	return ret_val;				// This is the requested data.
}

/************************************************************************/
/* REQUEST SENSOR DATA                                                  */
/*																		*/
/* @param: sender_id:	FROM-WHO, ex: EPS_TASK_ID						*/
/* @param: ssm_id:	Which SSM you are communicating with. ex: SUB1_ID0	*/
/* @Purpose: This function can be used to retrieve sensor data from an 	*/
/* SSM. 																*/
/* @return: < 0 == Failure, otherwise returns sensor value requested	*/
/* NOTE: This is for use with tasks and their corresponding SSMs only.	*/
/* NOTE: This function will wait for a maximum of 25ms. for the			*/
/* operation to complete.												*/
/************************************************************************/

uint32_t request_sensor_data(uint8_t sender_id, uint8_t ssm_id, uint8_t sensor_name, int* status)
{
	uint32_t ret_val = 0;
	uint32_t id;
	uint8_t* s;
	if(ssm_id == COMS_ID)
		id = SUB0_ID0;
	if(ssm_id == EPS_ID)
		id = SUB1_ID0;
	if(ssm_id == PAY_ID)
		id = SUB2_ID0;
	
	if (xSemaphoreTake(Can0_Mutex, (TickType_t) 0) == pdTRUE)		// Attempt to acquire CAN1 Mutex, block for 1 tick.
	{
		ret_val = request_sensor_data_h(sender_id, id, sensor_name, s);
		*status = (int)(*s);
		xSemaphoreGive(Can0_Mutex);
		return ret_val;
	}
	else
		return -1;					// CAN0 was busy or something has gone wrong.
}

/************************************************************************/
/* SET SENSOR DATA HIGH/LOW                                             */
/*																		*/
/* @param: sender_id:	FROM-WHO, ex: EPS_TASK_ID						*/
/* @param: ssm_id:	Which SSM you are communicating with. ex: SUB1_ID0	*/
/* @param: sensor_name: Name of the sensor at hand. ex: PANELX_V		*/
/* @param: boundary: The value which you would like to set as the high	*/
/*			or low value.												*/
/* @return: returns 1 upon success, -1 upon failure.					*/
/* @NOTE: This function checks to make sure that the request succeeded	*/
/*			(this has a timeout of 25 ms)								*/
/* @NOTE: This is for use with tasks and their corresponding SSMs only.	*/
/************************************************************************/
int set_sensor_high(uint8_t sender_id, uint8_t ssm_id, uint8_t sensor_name, uint16_t boundary)
{
	uint32_t high, low, check;
	uint8_t status;
	uint32_t id;

	if(ssm_id == COMS_ID)
		id = SUB0_ID0;
	if(ssm_id == EPS_ID)
		id = SUB1_ID0;
	if(ssm_id == PAY_ID)
		id = SUB2_ID0;
	
	high = high_command_generator(sender_id, ssm_id, MT_COM, SET_SENSOR_HIGH);
	low = (uint32_t)sensor_name;
	low = low << 24;
	low |= boundary;

	if (xSemaphoreTake(Can0_Mutex, (TickType_t) 0) == pdTRUE)		// Attempt to acquire CAN1 Mutex, block for 1 tick.
	{
		if(send_can_command_h(low, high, id, DEF_PRIO) < 1)
		{
			xSemaphoreGive(Can0_Mutex);
			return -1;
		}
		
		check = request_sensor_data_h(sender_id, id, sensor_name, &status);
		
		if ((status > 1) || (check != boundary))
		{
			xSemaphoreGive(Can0_Mutex);
			return -1;
		}
		else
		{
			xSemaphoreGive(Can0_Mutex);
			delay_us(100);
			return 1;
		}
	}
	else
		return -1;						// CAN0 is currently busy or something has gone wrong.
}

int set_sensor_low(uint8_t sender_id, uint8_t ssm_id, uint8_t sensor_name, uint16_t boundary)
{
	uint32_t high, low, check;
	uint8_t ret_val;
	uint32_t id;
	
	if(ssm_id == COMS_ID)
		id = SUB0_ID0;
	if(ssm_id == EPS_ID)
		id = SUB1_ID0;
	if(ssm_id == PAY_ID)
		id = SUB2_ID0;
	
	high = high_command_generator(sender_id, ssm_id, MT_COM, SET_SENSOR_LOW);
	low = (uint32_t)sensor_name;
	low = low << 24;
	low |= boundary;

	if (xSemaphoreTake(Can0_Mutex, (TickType_t) 0) == pdTRUE)		// Attempt to acquire CAN1 Mutex, block for 1 tick.
	{
		if(send_can_command_h(low, high, id, DEF_PRIO) < 1)
		{
			xSemaphoreGive(Can0_Mutex);
			return -1;
		}

		check = request_sensor_data_h(sender_id, id, sensor_name, &ret_val);
		
		if ((ret_val > 1) || (check != boundary))
		{
			xSemaphoreGive(Can0_Mutex);
			return -1;
		}
		else
		{
			xSemaphoreGive(Can0_Mutex);
			delay_us(100);
			return 1;
		}
	}
	else
		return -1;
}

/************************************************************************/
/* SET VARIABLE				                                            */
/*																		*/
/* @param: sender_id:	FROM-WHO, ex: EPS_TASK_ID						*/
/* @param: ssm_id:	Which SSM you are communicating with. ex: SUB1_ID0	*/
/* @param: sensor_name: Name of the variable at hand. ex: MPPTA			*/
/* @param: value: variable = value on SMM is the desired action.		*/
/* @return: returns 1 upon success, -1 upon failure.					*/
/* @NOTE: This function checks to make sure that the request succeeded	*/
/*			(this has a timeout of 25 ms)								*/
/* @NOTE: This is for use with tasks and their corresponding SSMs only.	*/
/************************************************************************/
int set_variable(uint8_t sender_id, uint8_t ssm_id, uint8_t var_name, uint16_t value)
{
	uint32_t high, low, check;
	uint8_t ret_val;
	uint32_t id;

	if(ssm_id == COMS_ID)
		id = SUB0_ID0;
	if(ssm_id == EPS_ID)
		id = SUB1_ID0;
	if(ssm_id == PAY_ID)
		id = SUB2_ID0;
	
	high = high_command_generator(sender_id, ssm_id, MT_COM, SET_VAR);
	low = (uint32_t)var_name;
	low = low << 24;
	low |= value;
	
	if (xSemaphoreTake(Can0_Mutex, (TickType_t) 0) == pdTRUE)		// Attempt to acquire CAN1 Mutex, block for 1 tick.
	{
		send_can_command_h(low, high, id, DEF_PRIO);
		check = request_sensor_data_h(sender_id, id, var_name, &ret_val);
		
		if ((ret_val > 1) || (check != value))
		{
			xSemaphoreGive(Can0_Mutex);
			return -1;
		}
		else
		{
			xSemaphoreGive(Can0_Mutex);
			delay_us(100);
			return 1;
		}
	}
	else
		return -1;					// CAN0 is currently busy or something has gone wrong.
}

// Let the SSM know that you're ready for a TC packet.
static void start_tc_packet(void)
{
	if((!receiving_tcf) && (!current_tc_fullf))
	{
		send_can_command(0x00, 0x00, OBC_PACKET_ROUTER_ID, COMS_ID, OK_START_TC_PACKET, COMMAND_PRIO);		
	}
	receiving_tcf = 1;
	return;
}

