/*
Author: Keenan Burnett
***********************************************************************
* FILE NAME: obc_packet_router.c
*
* PURPOSE:
* This file is to be used to house the OBC's Packet router task.
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
* TC = Telecommand (things sent up to the satellite)
* TM = Telemetry   (things sent down to ground)
*
* REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
* DEVELOPMENT HISTORY:
* 11/01/2015		Created.
*
* 11/03/2015		TC/TM transaction functions are now complete.
*
* 11/04/2015		Added a function for sending a TC verification.
*
* 11/05/2015		I am adding functionality so that this task can communicate 
*					with the housekeeping task via a command FIFO.
*
*					I am writing the function decode_telecommand() which shall take
*					what is stored in current_tc, and turn it into a command to a task or SSM.
*
*					I am including checksum.h so that I can do a CRC on the PUS packet.
*
* 11/06/2015		I created a helpfer function called verify_telecommand() which goes through
*					all the possible invalidities in the packet other than usage errors.
*					When an error is detected, a TC verify failure packet is created.
*					If all goes well, a TC verify accetptance packet is created.
*
*					I am also working on creating a helper function called decode_telecommand()
*					This shall do all the work of turning the incoming telecommands into 
*					actions, task commands, and CAN messages.
*
* DESCRIPTION:
* This task is in charge of managing communication requests from tasks on
* the OBC that wish to have something downlinked as well as dissecting the incoming
* packets and turning them into CAN messages to SSMs or setting flags in other
* tasks in order to request a particular action.
*
* The main reason why this is a task of it's own is that it needs a very high
* priority and so it would be a poor architecture choice to give the COMS task
* that much power.
*
* ESA's Packet Utilization Standard is used for communication back and forth with
* the grounstation.
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

/*		RTC includes.	*/
#include "rtc.h"

#include "can_func.h"

#include "global_var.h"

#include "checksum.h"

/* Priorities at which the tasks are created. */
#define OBC_PACKET_ROUTER_PRIORITY		( tskIDLE_PRIORITY + 5 )	// Shares highest priority with FDIR.

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define OBC_PACKET_ROUTER_PARAMETER		( 0xABCD )

/* Definitions to clarify which services represent what.		*/
#define TC_VERIFY_SERVICE				1
#define HK_SERVICE						3
#define EVENT_REPORT_SERVICE			5
#define MEMORY_SERVICE					6
#define TIME_SERVICE					9

/* Definitions to clarify which service subtypes represent what	*/
/* Housekeeping							*/
#define NEW_HK_DEFINITION				1
#define CLEAR_HK_DEFINITION				3
#define ENABLE_PARAM_REPORT				5
#define DISABLE_PARAM_REPORT			6
#define REPORT_HK_DEFINITIONS			9
#define HK_DEFINITON_REPORT				10
#define HK_REPORT						25

/*
 * Functions Prototypes.
 */
static void prvOBCPacketRouterTask( void *pvParameters );
void obc_packet_router(void);
static int packetize_send_telemetry(uint8_t sender, uint8_t dest, uint8_t service_type, uint8_t service_sub_type, uint8_t packet_sub_counter, uint16_t num_packets, uint8_t* data);
static int receive_tc_msg(void);
static int send_pus_packet_tm(uint8_t sender_id);
static void send_tc_transaction_response(uint8_t code);
static void clear_current_tc(void);
static void clear_current_data(void);
static void store_current_tc(void);
static int decode_telecommand(void);
static int decode_telecommand_h(uint8_t service_type, uint8_t service_sub_type);
static int send_tc_verification(uint16_t packet_id, uint16_t sequence_control, uint8_t status, uint8_t code, uint32_t parameter);
static int verify_telecommand(uint8_t packet_length, uint16_t pec0, uint16_t pec1, uint8_t service_type, uint8_t service_sub_type);static int verify_telecommand(uint8_t packet_length, uint16_t pec0, uint16_t pec1, uint8_t service_type, uint8_t service_sub_type, uint8_t version, uint8_t ccsds_flag, uint8_t packet_version);

/* Global variables											 */
static uint8_t version;															// The version of PUS we are using.
static uint8_t type, data_header, flag, sequence_flags, sequence_count;		// Sequence count keeps track of which packet (of several) is currently being sent.
uint16_t abs_time;															// Should count up from 0 the moment the satellite is deployed.
uint16_t packet_id, pcs; 
static uint8_t tc_sequence_count;
static uint32_t new_tc_msg_high, new_tc_msg_low;
static uint8_t tc_verify_success_counter, tc_verify_fail_counter;
static uint8_t current_data[DATA_LENGTH];
static uint8_t current_command[DATA_LENGTH + 10];
/* Latest TC packet received, next TM packet to send	*/
static uint8_t current_tc[PACKET_LENGTH + 1], current_tm[PACKET_LENGTH + 1];	// Arrays are 144B for ease of implementation.
static uint8_t tc_to_decode[PACKET_LENGTH + 1];
static TickType_t xTimeToWait;

/************************************************************************/
/* OBC_PACKET_ROUTER (Function)											*/
/* @Purpose: This function is used to create the obc packet router task	*/
/************************************************************************/
void obc_packet_router( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvOBCPacketRouterTask(),					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) OBC_PACKET_ROUTER_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					OBC_PACKET_ROUTER_PRIORITY, 			/* The priority assigned to the task. */
					NULL );								/* The task handle is not required, so NULL is passed. */
	return;
}
/*-----------------------------------------------------------*/

/************************************************************************/
/*				OBC_PACKET_ROUTER		                                */
/*	This task receives chunks of telecommand packets in FIFOs			*/
/* new_tc_msg_low and new_tc_msg_high. It then attempts to reconstruct	*/
/* the entire telecommand packet which was received by the COMS SSM.	*/
/* After which it will decode the telecommand and either send out the	*/
/* required immediate command or it will send a scheduling request to	*/
/* the scheduling process. This task can also receive telemetry requests*/
/* from other tasks which come in FIFOs which have yet to be defined.	*/
/************************************************************************/
static void prvOBCPacketRouterTask( void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == OBC_PACKET_ROUTER_PARAMETER );
	uint32_t new_tc_msg_high = 0, new_tc_msg_low = 0;
	int status;
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 10;	// Number entered here corresponds to the number of ticks we should wait.

	/* Initialize Global variables and flags */
	current_tc_fullf = 0;
	tc_sequence_count = 0;
	new_tc_msg_high = 0;
	new_tc_msg_low = 0;
	tc_verify_success_counter = 0;
	tc_verify_fail_counter = 0
	clear_current_data();
	clear_current_command();


	/* Initialize variable used in PUS Packets */
	version = 0;		// First 3 bits of the packet ID. (0 is default)
	data_header = 1;	// Include the data field header in the PUS packet.

	/* @non-terminating@ */	
	for( ;; )
	{
		if( xQueueReceive(tc_msg_fifo, &new_tc_msg_low, xTimeToWait) == pdTRUE)		// Block on the TC_MSG_FIFO for a maximum of 10 ticks.
		{
			xQueueReceive(tc_msg_fifo, &new_tc_msg_high, xTimeToWait);
			status = receive_tc_msg();					// FAILURE_RECOVERY if status == -1.
		}
		if(current_tc_fullf)
			decode_telecommand();

		if(xQueueReceive(hk_to_tm_fifo, current_command[0], (TickType_t)1) == pdTRUE)
		{
			if(current_command[137] == 0)
			{
				packetize_send_telemetry(HK_TASK_ID, HK_GROUND_ID, 1, 3, 25, 1, current_command[0]);
			}	
			if(current_command[137] == 1)
			{
				packetize_send_telemetry(HK_TASK_ID, HK_GROUND_ID, 1, 3, 10, 1, current_command[0]);
			}
		}
			
	}
}
/*-----------------------------------------------------------*/

// static helper functions may be defined below.

/************************************************************************/
/* PACKETIZE_SEND_TELEMETRY                                             */
/* 																		*/
/* @param: sender: The ID of the task which is sending this request,	*/
/* ex: OBC_PACKET_ROUTER_ID												*/
/* @param: dest: The ID of the SSM which we are going to send the TM	*/
/* packet to, ex: COMS_ID.												*/
/* @param: service_type: ex: TC verification = 1.						*/
/* @param: service_sub_type: ex: Define New Housekeeping Parameter		*/
/* Report => ST = 3, SST = 1											*/
/* @param: packet_sub_counter: Should be a global variable which is		*/
/* incremented every time a packet of this (ST/SST) is sent.			*/
/* @param: num_packet: The number of packets which you would like to	*/
/* send.																*/
/* @param: *dada: Array of 128 Bytes of data for the packet.			*/
/* @purpose: This function turns the parameters into a 143 B PUS packet */
/* and proceeds to send this packet to the COMS SSM so that it can be	*/
/* downlinked to the groundstation.										*/
/* @return: -1 == Failure, 36 == success.								*/
/************************************************************************/
static int packetize_send_telemetry(uint8_t sender, uint8_t dest, uint8_t service_type, uint8_t service_sub_type, uint8_t packet_sub_counter, uint16_t num_packets, uint8_t* data)
{
	uint16_t i, j;
	
	type = 0;			// Distinguishes TC and TM packets, TM = 0
	sequence_count = 0;
	uint16_t packet_error_control = 0;
	
	if(num_packets > 1)
		sequence_flags = 0x1;	// Indicates that this is the first packet in a series of packets.
	else
		sequence_flags = 0x3;	// Indicates that this is a standalone packet.
	
	// Packet Header
	current_tm[143] = 0x00;		// Extra byte to make the array 144 B.
	current_tm[142] = ((version & 0x07) << 5) | (type & 0x01);
	current_tm[141] = sender
	current_tm[140] = (sequence_flags & 0x03) << 6;
	current_tm[139] = sequence_count;
	current_tm[138] = 0x00;
	current_tm[137]	= PACKET_LENGTH - 1;	// Represents the length of the data field - 1.
	// Data Field Header
	current_tm[136] = (version & 0x07) << 4;
	current_tm[135] = service_type;
	current_tm[134] = service_sub_type;
	current_tm[133] = packet_sub_counter;
	current_tm[132] = dest;
	current_tm[131] = (uint8_t)((abs_time & 0xFF00) >> 8);
	current_tm[130] = (uint8_t)(abs_time & 0x00FF);
	// The Packet Error Control (PEC) is usually put at the end
	// this is usually a CRC on the rest of the packet.
	current_tm[1] = 0x00;
	current_tm[0] = 0x00;

	
	if(num_packets == 1)
	{
		sequence_count++;
		current_tm[139] = sequence_count;
		
		for(j = 2; j < (PACKET_LENGTH - 13); j++)
		{
			current_tm[j] = *(data + (j - 2));
		}
		
		/* Run checksum on the PUS packet	*/
		packet_error_control = fletcher16(current_tm[2], 141);
		current_tm[1] = (uint8_t)(packet_error_control >> 8);
		current_tm[0] = (uint8_t)(packet_error_control & 0x00FF);
		
		if( send_pus_packet_tm(sender) < 0)
		return 0;
		
		return 1;
	}
	
	for(i = 0; i < num_packets; i++)
	{
		sequence_count++;
		current_tm[139] = sequence_count;
		
		if(i > 1)
		sequence_flags = 0x0;			// Continuation packet
		if(i == (num_packets - 1))
		sequence_flags = 0x2;			// Last packet
		current_tm[140] = (sequence_flags & 0x03) << 6;
		
		for(j = 2; j < (PACKET_LENGTH - 13); j++)
		{
			current_tm[j] = *(data + (j - 2) + (i * 128));
		}
		
		/* Run checksum on the PUS packet	*/
		packet_error_control = fletcher16(current_tm[2], 141);
		current_tm[1] = (uint8_t)(packet_error_control >> 8);
		current_tm[0] = (uint8_t)(packet_error_control & 0x00FF);
		
		if(send_pus_packet_tm(sender) < 0)
		return i;
	}
	
	return num_packets;
}

static int receive_tc_msg(void)
{
	uint8_t ssm_seq_count = (uint8_t)(new_tc_msg_high & 0x000000FF);
	
	if(ssm_seq_count > (tc_sequence_count + 1))
	{
		send_tc_transaction_response(0xFF);
		tc_sequence_count = 0;
		receiving_tcf = 0;
		clear_current_tc();
		return -1;
	}
	if(current_tc_fullf)
	{
		send_tc_transaction_response(0xFF);
		tc_sequence_count = 0;
		receiving_tcf = 0;
		return -1;
	}
	
	if((!ssm_seq_count && !tc_sequence_count) || (ssm_seq_count == (tc_sequence_count + 1)))
	{
		tc_sequence_count = ssm_seq_count;
		receiving_tcf = 1;
		current_tc[(ssm_seq_count * 4)] = (uint8_t)((new_tc_msg_low & 0x000000FF));
		current_tc[(ssm_seq_count * 4) + 1] = (uint8_t)((new_tc_msg_low & 0x0000FF00) << 8);
		current_tc[(ssm_seq_count * 4) + 2] = (uint8_t)((new_tc_msg_low & 0x00FF0000) << 16);
		current_tc[(ssm_seq_count * 4) + 3] = (uint8_t)((new_tc_msg_low & 0xFF000000) << 24);
		if(ssm_seq_count == 35)
		{
			tc_sequence_count = 0;
			receiving_tcf = 0;
			current_tc_fullf = 1;
			store_current_tc();
			send_tc_transaction_response(ssm_seq_count);
		}
		return ssm_seq_count;
	}
	else
	{
		send_tc_transaction_response(0xFF);
		tc_sequence_count = 0;
		receiving_tcf = 0;
		clear_current_tc();
		return -1;
	}
}

// This function breaks down the PUS packet into multiple CAN messages
// that are sent in turn and then placed into a telemetry buffer on the side of the SSM.

// Note: It is assumed that the PUS packet shall be located in current_tm which is defined in glob_var.h

// Instead of putting time in Byte 4, I'm going to use it for sequence control so that the SSM can check
// to make sure that no packets were lost.

// It should also be obvious that this packet is being sent to the COMS SSM.

// DO NOT call this function from an ISR.

static int send_pus_packet_tm(uint8_t sender_id)
{
	uint32_t low, i;
	uint32_t num_transfers = PACKET_LENGTH / 4;
	uint8_t timeout = 25;
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 10;
	
	tm_transfer_completef = 0;
	start_tm_transferf = 0;
	send_can_command(0x00, 0x00, sender_id, COMS_ID, TM_PACKET_READY, COMMAND_PRIO);	// Let the SSM know that a TM packet is ready.
	while(!start_tm_transferf)					// Wait for ~25 ms, for the SSM to say that we're good to start/
	{
		if(!timeout--)
		{
			return -1;
			taskYIELD();
		}
	}
	start_tm_transferf = 0;
	timeout = 100;
	
	for(i = 0; i < num_transfers; i++)
	{
		if(tm_transfer_completef == 0xFF)			// The transaction has failed.
			return -1;
		low =	(uint32_t)current_tm[(i * 4)];			// Place the data into the lower 4 bytes of the CAN message.
		low += (uint32_t)(current_tm[(i * 4) + 1] << 8);
		low += (uint32_t)(current_tm[(i * 4) + 2] << 16);
		low += (uint32_t)(current_tm[(i * 4) + 3] << 24);
		send_can_command(low, i, sender_id, COMS_ID, SEND_TM, COMMAND_PRIO);
		xLastWakeTime = xTaskGetTickCount();		// Causes a mandatory delay of at least 10ms (10 * 1ms)
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
	}
	
	while(!tm_transfer_completef)					// Delay for ~100 ms for the SSM to let the OBC know that
	{												// the transfer has completed.
		if(!timeout--)
		{
			return -1;
			taskYIELD();			
		}
	}
	
	if(tm_transfer_completef != 35)
	{
		tm_transfer_completef = 0;
		return -1;
	}
	else
	{
		tm_transfer_completef = 0;
		return tm_transfer_completef;
	}
}

static void send_tc_transaction_response(uint8_t code)
{
	uint32_t low;
	low = (uint32_t)code;	
	send_can_command(low, CURRENT_MINUTE, OBC_PACKET_ROUTER_ID, COMS_ID, TC_TRANSACTION_RESP, COMMAND_PRIO);
	return;
}

static void clear_current_tc(void)
{
	uint8_t i;
	for(i = 0; i < PACKET_LENGTH; i++)
	{
		current_tc[i] = 0;
	}
	return;
}

static void clear_current_data(void)
{
	uint8_t i;
	for(i = 0; i < DATA_LENGTH; i++)
	{
		current_data[i] = 0;
	}
	return;
}

static void clear_current_command(void)
{
	uint8_t i;
	for(i = 0; i < (DATA_LENGTH + 10); i++)
	{
		current_command[i] = 0;
	}
	return;
}

static void store_current_tc(void)
{
	uint8_t i;
	for(i = 0; i < PACKET_LENGTH; i++)
	{
		tc_to_decode[i] = current_tc[i];
	}
}

// This function assumes that the telecommand to decode is contained in tc_to_decode[144].
// current_tc_fullf should also be 1 before executing this function.
static int decode_telecommand(void)
{
	uint8_t data_field_headerf, apid;
	int x;
	uint32_t parameter = 0;
	uint8_t packet_length;
	uint16_t pec1, pec0;
	uint8_t ack, service_type, service_sub_type, source_id;
	uint8_t version1, type1, sequence_flags1, sequence_count1;
	uint8_t ccsds_flag, packet_version;
	
	if(!current_tc_fullf)
		return -1;
	
	packet_id = (uint16_t)(current_tc[139]);
	packet_id = packet_id << 8;
	packet_id |= (uint16_t)(current_tc[138]);
	
	pcs = (uint16_t)(current_tc[137]);
	pcs = pcs << 8;
	pcs |= (uint16_t)(current_tc[136]);
	
	// PACKET HEADER
	version1			= (current_tc[139] & 0xE0) >> 5;
	type1				= (current_tc[139] & 0x10) >> 4;
	data_field_headerf	= (current_tc[139] & 0x08) >> 3;
	apid				= current_tc[138];
	sequence_flags1		= (current_tc[137] & 0xC0) >> 6;
	sequence_count1		= current_tc[136];
	packet_length		= current_tc[134] + 1;				// B137 = PACKET_LENGTH - 1
	// DATA FIELD HEADER
	ccsds_flag			= (current_tc[133] & 0X80) >> 7;
	packet_version		= (current_tc[133] & 0X70) >> 4;
	ack					= current_tc[133] & 0X0F;
	service_type		= current_tc[132];
	service_sub_type	= current_tc[131];
	source_id			= current_tc[130];
	
	pec1 = (uint16_t)(current_tc[1]);
	pec1 = packet_error_control << 8;
	pec1 &= (uint16_t)(current_tc[0]);
	
	/* Check that the packet error control is correct		*/
	pec0 = fletcher16(current_tc[2], 141);
	/* Verify that the telecommand is ready to be decoded.	*/
	x = verify_telecommand(packet_length, pec0, pec1, service_type, service_sub_type, version1, ccsds_flag, packet_version);		// FAILURE_RECOVERY required if x == -1.
	if(x < 0)
		return -1;
	
	/* Decode the telecommand packet						*/		// To be updated on a rolling basis
	decode_telecommand_h(service_type, service_sub_type);
	
	return;
}

// This function looks at the service type and service substype of the incoming telecommand and turns it into 
// either a task command, action, or CAN message.
static int decode_telecommand_h(uint8_t service_type, uint8_t service_sub_type)
{	
	uint8_t sID = 0xFF;
	uint8_t collection_interval = 0;
	uint8_t npar1 = 0;
	uint8_t i;
	
	clear_current_command();
	for(i = 0; i < DATA_LENGTH; i++)
	{
		current_command[i] = current_tc[i];
	}
	
	if(service_type == HK_SERVICE)
	{
		switch(service_sub_type)
		{
			case	NEW_HK_DEFINITION:
				sID = current_tc[129];			// Structure ID for this definition.
				if(sID != 1)
				{
					send_tc_verification(packet_id, pcs, 0xFF, 5, 0x00);				// Only sID of 1 is allowed.
					return -1;
				}
				collection_interval = (uint32_t)current_tc[128];	
				npar1 = current_tc[127];
				
				if(npar1 > 64)
				{
					send_tc_verification(packet_id, pcs, 0xFF, 5, 0x00);				// Npar1 must be <= 64
					return -1;
				}
				
				current_command[137] = HK_SERVICE;
				current_command[136] = collection_interval;
				current_command[135] = npar1;
				
				for (i = 0; i < DATA_LENGTH; i++)
				{
					current_command[i] = current_tc[i + 2];
				}
				
				xQueueSendToBack(obc_to_hk_fifo, &current_command, (TickType_t)1);		// FAILURE_RECOVERY if this doesn't return pdTrue.
				
			case	CLEAR_HK_DEFINITION:
				sID = current_tc[129];
				if(sID != 1)
				{
					send_tc_verification(packet_id, pcs, 0xFF, 5, 0x00);				// Usage error.
					return -1;
				}
				current_command[137] = CLEAR_HK_DEFINITION;
				xQueueSendToBack(obc_to_hk_fifo, &current_command, (TickType_t)1);
			case	ENABLE_PARAM_REPORT:
				current_command[137] = ENABLE_PARAM_REPORT;
				xQueueSendToBack(obc_to_hk_fifo, &current_command, (TickType_t)1);
			case	DISABLE_PARAM_REPORT:
				current_command[137] = DISABLE_PARAM_REPORT;
				xQueueSendToBack(obc_to_hk_fifo, &current_command, (TickType_t)1);
			case	REPORT_HK_DEFINITIONS:
				current_command[137] = REPORT_HK_DEFINITIONS;
				xQueueSendToBack(obc_to_hk_fifo, &current_command, (TickType_t)1);
			default:
				break;
		}
	}
	
	return;
}

// This helper function is going to be used to determine whether or not the received telecommand is valid.
// FAILURE_RECOVERY required if any of the send_tc_verification functions returns -1.
static int verify_telecommand(uint8_t packet_length, uint16_t pec0, uint16_t pec1, uint8_t service_type, uint8_t service_sub_type, uint8_t version, uint8_t ccsds_flag, uint8_t packet_version)
{
	int x;
	if(packet_length != PACKET_LENGTH)
	{
		x = send_tc_verification(packet_id, pcs, 0xFF, 1, (uint32_t)packet_length);		// TC verify acceptance report, failure, 1 == invalid packet length
		return -1;
	}
	
	if(pec0 != pec1)
	{
		x = send_tc_verification(packet_id, pcs, 0xFF, 2, (uint32_t)pec1);				// TC verify acceptance report, failure, 2 == invalid PEC (checksum)
		return -1;
	}
	
	if((service_type != 3) && (service_type != 6) && (service_type != 9))
	{
		x = send_tc_verification(packet_id, pcs, 0xFF, 3, (uint32_t)service_type);		// TC verify acceptance report, failure, 3 == invalid service type
		return -1;
	}
	
	if(service_type == 3)
	{
		if((service_sub_type != 1) && (service_sub_type != 2) && (service_sub_type != 3) && (service_sub_type != 4) && (service_sub_type != 5) && (service_sub_type != 6)
		&& (service_sub_type != 7) && (service_sub_type != 8) && (service_sub_type != 9) && (service_sub_type != 11) && (service_sub_type != 17) && (service_sub_type != 18))
		{
			x = send_tc_verification(packet_id, pcs, 0xFF, 4, (uint32_t)service_sub_type);	// TC verify acceptance report, failure, 4 == invalid service subtype.
			return -1;
		}
		
		if((apid != HK_TASK_ID) && (apid != FDIR_TASK_ID))
		{
			x = send_tc_verification(packet_id, pcs, 0xFF, 0, (uint32_t)apid);				// TC verify acceptance report, failure, 0 == invalid apid
			return -1
		}
	}
	
	if(service_type == 6)
	{
		if((service_sub_type != 2) && (service_sub_type != 5) && (service_sub_type != 9))
		{
			x = send_tc_verification(packet_id, pcs, 0xFF, 4, (uint32_t)service_sub_type);	// TC verify acceptance report, failure, 4 == invalid service subtype
			return -1;
		}
		if(apid != MEMORY_TASK_ID)
		{
			x = send_tc_verification(packet_id, pcs, 0xFF, 0, (uint32_t)apid);				// TC verify acceptance report, failure, 0 == invalid apid
			return -1;
		}
	}
	
	if(service_type == 9)
	{
		if(service_sub_type != 1)
		{
			x = send_tc_verification(packet_id, pcs, 0xFF, 4, (uint32_t)service_sub_type);		// TC verify acceptance report, failure, 4 == invalid service subtype
			return -1;			
		}
		if(apid != TIME_TASK_ID)
		{
			x = send_tc_verification(packet_id, pcs, 0xFF, 0, (uint32_t)apid);				// TC verify acceptance report, failure, 0 == invalid apid
			return -1;
		}
	}
	
	if(!version)
	{
		x = send_tc_verification(packet_id, pcs, 0xFF, 5, 0x00);							// TC verify acceptance repoort, failure, 5 == usage error
		return -1;
	}
	
	if(ccsds_flag != 1)
	{
		x = send_tc_verification(packet_id, pcs, 0xFF, 5, 0x00);							// TC verify acceptance report, failure, 5 == usage error
		return -1;
	}
	
	if(packet_version != 1)
	{
		x = send_tc_verification(packet_id, pcs, 0xFF, 5, 0x00);							// TC verify acceptance report, failure, 5 == usage error
		return -1;
	}
	
	/* The telecommand packet is good to be decoded further!		*/
	x = send_tc_verification(packet_id, pcs, 0, 0, 0);										// TC verify acceptance report, success.
	return 1;
}


// This function turns the parameters into a TC verification packet and proceeds to send it
// to the COMS SSM so that it can be downlinked.
// status: 0 = SUCCESS, >0 = FAILURE
// @return: -1 == FAILURE, 1 == SUCCESS
static int send_tc_verification(uint16_t packet_id, uint16_t sequence_control, uint8_t status, uint8_t code, uint32_t parameter)
{
	int resp = -1;
	if (status > 1)
		return -1;				// Invalid Status inputted.
	if (code > 5)
		return -1;				// Invalid code inputted.

	clear_current_data();
	
	if(!status)					// Telecommand Acceptance Report - Success (1,1)
	{
		tc_verify_success_counter++;
		current_data[0] = (uint8_t)(sequence_control & 0x00FF);
		current_data[1] = (uint8_t)((sequence_control & 0xFF00) >> 8);
		current_data[2] = (uint8_t)(packet_id & 0x00FF);
		current_data[3] = (uint8_t)((packet_id & 0xFF00) >> 8);
		resp = packetize_send_telemetry(OBC_PACKET_ROUTER_ID, COMS_ID, 1, 1, tc_verify_success_counter, 1, current_data);

		if(resp == -1)
			return -1;
		else
			return 1;		
	}
	else						// Telecommand Acceptance Report - Failure (1,2)
	{
		tc_verify_fail_counter++;
		current_data[0] = (uint8_t)((parameter & 0x000000FF));
		current_data[1] = (uint8_t)((parameter & 0x0000FF00) >> 8);
		current_data[2] = (uint8_t)((parameter & 0x00FF0000) >> 16);
		current_data[3] = (uint8_t)((parameter & 0xFF000000) >> 24);
		current_data[4] = code;
		current_data[5] = (uint8_t)(sequence_control & 0x00FF);
		current_data[6] = (uint8_t)((sequence_control & 0xFF00) >> 8);
		current_data[7] = (uint8_t)(packet_id & 0x00FF);
		current_data[8] = (uint8_t)((packet_id & 0xFF00) >> 8);		
		resp = packetize_send_telemetry(OBC_PACKET_ROUTER_ID, COMS_ID, 1, 2, tc_verify_success_counter, 1, current_data);
		current_tc_fullf = 0;
		if(resp == -1)
			return -1;
		else
			return 1;
	}
}
