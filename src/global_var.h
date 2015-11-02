/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		global_var.h
	*
	*	PURPOSE:		This file contains global variable definitions to be used across all files.
	*	
	*
	*	FILE REFERENCES:		stdio.h
	*
	*	EXTERNAL VARIABLES:		None
	*
	*	EXTERNAL REFERENCES:	Same a File References.
	*
	*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
	*
	*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
	*
	*	NOTES:					All future additions to this list of global variables should have
	*							'glob' somewhere in their name so that it is obvious that they are
	*							global.
	*
	*							The need for arrays that store can message information because tasks
	*							may require the data contained in a can mailbox outside of the interrupt
	*							handler. It doesn't make sense to reacquire the information from the 
	*							CAN controller every time it is required because we're adding much more
	*							code and complexity. 
	*
	*							There shouldn't be any concurrency issues, although just to be safe:
	*							ALL TASKS MUST ACQUIRE THE CAN MUTEX LOCK BEFORE READING CAN GLOBAL REGS.
	*
	*							Not all global variables are contained within this file.
	*
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
	*
	*	DEVELOPMENT HISTORY:
	*	03/29/2015			Created.
	*
	*	08/07/2015			Changed glob_comf to glob_comsf as that's less confusing.
	*
*/

#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"

/*		PUS DEFINITIONS HERE			*/
#define PACKET_LENGTH	143

/*  CAN GLOBAL FIFOS			   */
QueueHandle_t can_data_fifo;			// Initialized in can_initialize
QueueHandle_t can_msg_fifo;
QueueHandle_t can_hk_fifo;
QueueHandle_t can_com_fifo;
QueueHandle_t tc_msg_fifo;

/*	DATA RECEPTION FLAG			   */
uint8_t	glob_drf;							// Initialized in can_initialize
uint8_t glob_comsf;

/* HOUSEKEEPING COMMAND FLAG	   */
uint8_t hk_read_requestedf;
uint8_t hk_read_receivedf;
uint8_t hk_write_requestedf;
uint8_t hk_write_receivedf;

/* DATA RECEPTION FLAGS			  */
uint8_t eps_data_receivedf;
uint8_t coms_data_receivedf;
uint8_t pay_data_receivedf;

/* HOUSEKEEPING COMMAND RESPONSE REGS */
uint32_t hk_read_receive[2];
uint32_t hk_write_receive[2];

/* DATA RECEPTION REGS				  */
uint32_t eps_data_receive[2];
uint32_t coms_data_receive[2];
uint32_t pay_data_receive[2];

/*	DATA STORAGE POITNER		   */
uint32_t  glob_stored_data[2];			// Initialized in can_initialize
uint32_t  glob_stored_message[2];		// Initialized in can_initialize

uint32_t  SAFE_MODE;					// Condition which will initially hold the system in safe_mode.

uint8_t CURRENT_MINUTE;

/* Latest TC packet received, next TM packet to send	*/
uint8_t current_tc[PACKET_LENGTH], current_tm[PACKET_LENGTH];

/* TC/TM Packet flags									*/
uint8_t tm_transfer_completef;
uint8_t start_tm_transferf;
uint8_t current_tc_fullf, receiving_tcf;

