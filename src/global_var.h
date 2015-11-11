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
	*	11/05/2015			Adding lots of FIFOs for intertask communication
	*
*/

#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"

/*		PUS DEFINITIONS HERE			*/
#define PACKET_LENGTH	152
#define DATA_LENGTH		137

/*  CAN GLOBAL FIFOS				*/
/* Initialized in can_initialize()	*/
QueueHandle_t can_data_fifo;			// CAN Handler	-->		data_collect
QueueHandle_t can_msg_fifo;				// CAN Handler	-->		data_collect
QueueHandle_t can_hk_fifo;				// CAN Handler	-->		housekeep
QueueHandle_t can_com_fifo;				// CAN Handler	-->		command_test
QueueHandle_t tc_msg_fifo;				// CAN Handler	-->		obc_packet_router

/* PUS PACKET FIFOS					*/
/* Initialized in can_initialize()	*/
QueueHandle_t hk_to_obc_fifo;			// housekeep	-->		obc_packet_router
QueueHandle_t time_to_obc_fifo;			// time_manage	-->		obc_packet_router
QueueHandle_t mem_to_obc_fifo;			// memory		-->		obc_packet_router
QueueHandle_t sched_to_obc_fifo;		// scheduling	-->		obc_packet_router

/* GLOBAL COMMAND FIFOS				*/
/* Initializes in can_initialize()	*/
QueueHandle_t obc_to_hk_fifo;			// obc_packet_router	-->		housekeep
QueueHandle_t obc_to_time_fifo;			// obc_packet_router	-->		time_manage
QueueHandle_t obc_to_mem_fifo;			// obc_packet_router	-->		memory
QueueHandle_t obc_to_sched_fifo;		// obc_packet_router	-->		scheduling

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

/* TC/TM Packet flags									*/
uint8_t tm_transfer_completef;
uint8_t start_tm_transferf;
uint8_t current_tc_fullf, receiving_tcf;

/* Global variables for time management	*/
uint8_t ABSOLUTE_DAY;
uint8_t CURRENT_HOUR;
uint8_t CURRENT_MINUTE;
uint8_t CURRENT_SECOND;
uint8_t absolute_time_arr[4];
