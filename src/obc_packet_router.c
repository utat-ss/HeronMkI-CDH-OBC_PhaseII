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
*					If all goes well, a TC verify acceptance packet is created.
*
*					I am also working on creating a helper function called decode_telecommand()
*					This shall do all the work of turning the incoming telecommands into 
*					actions, task commands, and CAN messages.
*
* 11/07/2015		time_manage is now continuously updating the absolute time which is stored in
*					global variable absolute_time_arr[4].
*					I updated packetize_send_telemetry so that it uses the absolute time.
*
*					I needed to increase the size of the packets from 143B to 152B so that I could
*					fit in 128B for memory dumps and memory loads.
*
* 11/12/2015		Adding a bit more code to implement TC verification upon command completion as well
*					as adding some code so that we can implement event reporting (events to report 
*					shall come up over time.)
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
* the groundstation.
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
#define OBC_PACKET_ROUTER_PRIORITY		( tskIDLE_PRIORITY + 2 )	// Shares highest priority with FDIR.

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define OBC_PACKET_ROUTER_PARAMETER		( 0xABCD )

#define DEPLOY_TIMEOUT 60000

/* Functions Prototypes. */
static void prvOBCPacketRouterTask( void *pvParameters );
TaskHandle_t obc_packet_router(void);
void opr_kill(uint8_t killer);
static int packetize_send_telemetry(uint8_t sender, uint8_t dest, uint8_t service_type, uint8_t service_sub_type, uint8_t packet_sub_counter, uint16_t num_packets, uint8_t* data);
static int receive_tc_msg(void);
static int send_pus_packet_tm(uint8_t sender_id);
static void send_tc_transaction_response(uint8_t code);
static void clear_current_tc(void);
static void clear_current_data(void);
static void clear_current_command(void);
static int store_current_tc(void);
static int decode_telecommand(void);
static int decode_telecommand_h(uint8_t service_type, uint8_t service_sub_type);
static int send_tc_verification(uint16_t packet_id, uint16_t sequence_control, uint8_t status, uint8_t code, uint32_t parameter, uint8_t tc_type);
static int verify_telecommand(uint8_t apid, uint8_t packet_length, uint16_t pec0, uint16_t pec1, uint8_t service_type, uint8_t service_sub_type, uint8_t version, uint8_t ccsds_flag, uint8_t packet_version);
static void exec_commands(void);
static void send_event_packet(uint8_t sender, uint8_t severity);
static int store_current_tm(void);
static void send_event_report(uint8_t severity, uint8_t report_id, uint8_t param1, uint8_t param0);

void set_obc_variable(uint8_t parameter, uint32_t val);
uint32_t get_obc_variable(uint8_t parameter);

extern uint8_t get_ssm_id(uint8_t sensor_name);

/* Global variables											 */
static uint8_t version;															// The version of PUS we are using.
static uint8_t type, data_header, sequence_flags, sequence_count;		// Sequence count keeps track of which packet (of several) is currently being sent.
static uint16_t packet_id, psc;
static uint8_t tc_sequence_count, hk_telem_count, hk_def_report_count, time_report_count, mem_dump_count;
static uint8_t science_packet_count;
static uint8_t diag_telem_count, diag_def_report_count, sin_par_rep_count;
static uint8_t tc_exec_success_count, tc_exec_fail_count, mem_check_count;
static uint32_t new_tc_msg_high, new_tc_msg_low;
static uint8_t tc_verify_success_count, tc_verify_fail_count, event_report_count, sched_report_count, sched_command_count;
static uint8_t current_data[DATA_LENGTH];
static uint8_t current_command[DATA_LENGTH + 10];
static uint32_t high, low;
static uint16_t abs_time, pec, packet_error_control = 0;
static uint32_t num_transfers;
static uint16_t timeout;
static TickType_t xLastWakeTime;
static TickType_t xTimeToWait;
static uint8_t data_field_headerf, apid, packet_length;
static uint16_t pec1, pec0;
static uint8_t ack, service_type, service_sub_type, source_id;
static uint8_t version1, type1, sequence_flags1, sequence_count1;
static uint8_t ccsds_flag, packet_version;
static uint8_t sID, ssmID;
static uint8_t collection_interval;
static uint8_t npar1;
static uint32_t address, length;
static uint32_t new_time, last_time;
/* Latest TC packet received, next TM packet to send	*/
static uint8_t current_tc[PACKET_LENGTH], current_tm[PACKET_LENGTH];	// Arrays are 144B for ease of implementation.
static uint8_t tc_to_decode[PACKET_LENGTH], tm_to_downlink[PACKET_LENGTH];
static uint32_t low_received, high_received;
static uint32_t new_tc_msg_high, new_tc_msg_low;
static uint32_t deployed_antenna;

/************************************************************************/
/* OBC_PACKET_ROUTER (Function)											*/
/* @Purpose: This function is used to create the obc packet router task	*/
/************************************************************************/
TaskHandle_t obc_packet_router( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		TaskHandle_t temp_HANDLE = 0;
		xTaskCreate( prvOBCPacketRouterTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE * 5, 			/* The size of the stack to allocate to the task. */
					( void * ) OBC_PACKET_ROUTER_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					OBC_PACKET_ROUTER_PRIORITY, 			/* The priority assigned to the task. */
					&temp_HANDLE );								/* The task handle is not required, so NULL is passed. */
	return temp_HANDLE;
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

	int status;
	/* Initialize Global variables and flags */
	current_tc_fullf = 0;
	tc_sequence_count = 0;
	new_tc_msg_high = 0;
	new_tc_msg_low = 0;
	tc_verify_success_count = 0;
	hk_telem_count = 0;
	hk_def_report_count = 0;
	diag_telem_count = 0;
	diag_def_report_count = 0;
	tc_verify_fail_count = 0;
	tc_exec_success_count = 0;
	tc_exec_fail_count = 0;
	time_report_count = 0;
	mem_dump_count = 0;
	event_report_count = 0;
	sched_report_count = 0;
	sched_command_count = 0;
	mem_check_count = 0;
	sin_par_rep_count = 0;
	science_packet_count = 0;
	time_of_deploy = 0;
	deployed_antenna = 0;
	clear_current_data();
	clear_current_command();
	//task_spimem_read(OBC_PACKET_ROUTER_ID, TM_BASE, &TM_PACKET_COUNT, 4);	// FAILURE_HANDLING
	//task_spimem_read(OBC_PACKET_ROUTER_ID, TM_BASE + 4, &NEXT_TM_PACKET, 4);
	//task_spimem_read(OBC_PACKET_ROUTER_ID, TM_BASE + 8, &CURRENT_TM_PACKET, 4);
	//task_spimem_read(OBC_PACKET_ROUTER_ID, TC_BASE, &TC_PACKET_COUNT, 4);	// FAILURE_HANDLING
	//task_spimem_read(OBC_PACKET_ROUTER_ID, TC_BASE + 4, &NEXT_TC_PACKET, 4);
	//task_spimem_read(OBC_PACKET_ROUTER_ID, TC_BASE + 8, &CURRENT_TC_PACKET, 4);
	//if(NEXT_TM_PACKET == 0)
	//{
		//NEXT_TM_PACKET = TM_BASE + 12;
		//task_spimem_write(OBC_PACKET_ROUTER_ID, TM_BASE + 4, &NEXT_TM_PACKET, 4);
	//}
	//if(CURRENT_TM_PACKET == 0)
	//{
		//CURRENT_TM_PACKET = TM_BASE + 12;
		//task_spimem_write(OBC_PACKET_ROUTER_ID, TM_BASE + 8, CURRENT_TM_PACKET, 4);
	//}
	//if(NEXT_TC_PACKET == 0)
	//{
		//NEXT_TC_PACKET = TC_BASE + 12;
		//task_spimem_write(OBC_PACKET_ROUTER_ID, TC_BASE + 4, &NEXT_TC_PACKET, 4);
	//}
	//if(CURRENT_TC_PACKET == 0)
	//{
		//CURRENT_TC_PACKET = TC_BASE + 12;
		//task_spimem_write(OBC_PACKET_ROUTER_ID, TC_BASE + 8, CURRENT_TC_PACKET, 4);
	//}

	/* Initialize variable used in PUS Packets */
	version = 0;		// First 3 bits of the packet ID. (0 is default)
	data_header = 1;	// Include the data field header in the PUS packet.
	low_received = 0, high_received = 0;
	new_tc_msg_high = 0, new_tc_msg_low = 0;
	/* @non-terminating@ */	
	for( ;; )
	{
		if(!low_received)
		{
			if(xQueueReceive(tc_msg_fifo, &new_tc_msg_low, xTimeToWait) == pdTRUE)
				low_received = 1;
		}
		if(low_received & !high_received)
		{
			if(xQueueReceive(tc_msg_fifo, &new_tc_msg_high, xTimeToWait) == pdTRUE)
				high_received = 1;
		}
		if(high_received)		// Block on the TC_MSG_FIFO for a maximum of 10 ticks.
		{
			status = receive_tc_msg();					// FAILURE_RECOVERY if status == -1.
		}
		//if(TC_PACKET_COUNT && spimem_read(NEXT_TC_PACKET, tc_to_decode, PACKET_LENGTH) == PACKET_LENGTH)
		//{
			//TC_PACKET_COUNT--;
			//spimem_write(TC_BASE, &TC_PACKET_COUNT, 4);		// FAILURE_RECOVERY
			//NEXT_TC_PACKET += PACKET_LENGTH;
			//if (NEXT_TC_PACKET > (TC_BASE + 0x20000))
				//NEXT_TC_PACKET = TC_BASE + 12;
			//spimem_write(TC_BASE + 4, &NEXT_TC_PACKET, 4);	// Update the position of the next packet		
			//decode_telecommand();
		//}
		if(antenna_deploy == 1)
		{
			send_can_command(0, 0, OBC_PACKET_ROUTER_ID, EPS_ID, DEP_ANT_COMMAND, DEF_PRIO);
			if(xTaskGetTickCount() - time_of_deploy > DEPLOY_TIMEOUT)
			{
				send_can_command(0, 0, OBC_PACKET_ROUTER_ID, EPS_ID, DEP_ANT_OFF, DEF_PRIO);
				antenna_deploy = 0;
			}
			xTimeToWait = 100; // Sleep task for 100 ticks.
			xLastWakeTime = xTaskGetTickCount();		
		}

		if(!receiving_tcf)
		{
			if(xQueueReceive(tc_buffer, tc_to_decode, (TickType_t)1) == pdTRUE)
				decode_telecommand();
			if (tm_down_fullf)
			{
				send_pus_packet_tm(tm_to_downlink[150]);		// FAILURE_RECOVERY			
			}
			else if(xQueueReceive(tm_buffer, tm_to_downlink, (TickType_t)1) == pdTRUE)
			{
				tm_down_fullf = 1;
				send_pus_packet_tm(tm_to_downlink[150]);		// FAILURE_RECOVERY
			}
			if(tm_transfer_completef)
			{
				xTimeToWait = 3000; // Sleep task for 100 ticks.
				xLastWakeTime = xTaskGetTickCount();				
				vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
				tm_transfer_completef = 0;
			}
				
		
			//if (tm_down_fullf)
			//{
				//send_pus_packet_tm(tm_to_downlink[150]);		// FAILURE_RECOVERY			
			//}
			//else if(TM_PACKET_COUNT && (task_spimem_read(OBC_PACKET_ROUTER_ID, NEXT_TM_PACKET, tm_to_downlink, 152) > 0))
			//{
				//TM_PACKET_COUNT--;	// Update packet count.
				//task_spimem_write(OBC_PACKET_ROUTER_ID, TM_BASE, &TM_PACKET_COUNT, 4);		// FAILURE_RECOVERY
				//NEXT_TM_PACKET += 152;
				//if (NEXT_TM_PACKET > (TM_BASE + 0x20000))
					//NEXT_TM_PACKET = TM_BASE + 12;
				//task_spimem_write(OBC_PACKET_ROUTER_ID, TM_BASE + 4, &NEXT_TM_PACKET, 4);	// Update the position of the next packet 
				//tm_down_fullf = 1;
				//send_pus_packet_tm(tm_to_downlink[150]);		// FAILURE_RECOVERY
			//}	
			exec_commands();


			//vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
		}
	}
}
/*-----------------------------------------------------------*/
/* static helper functions below */

/************************************************************************/
/* EXEC_COMMANDS		                                                */
/* @Purpose: This function checks all the fifos which other PUS services*/
/* or tasks use to communicate with the OBC_PACKET_ROUTER. These are	*/
/* used so that other PUS services and tasks can downlink telemetry		*/
/* packets such as TC verification, event reports, or other	TM.			*/
/************************************************************************/
static void exec_commands(void)
{
	high = 0;
	low = 0;
	clear_current_command();
	if(xQueueReceive(hk_to_obc_fifo, current_command, (TickType_t)1) == pdTRUE)	// Check to see if there is a command from HK and execute it.
	{
		packet_id = ((uint16_t)current_command[140]) << 8;
		packet_id += (uint16_t)current_command[139];
		psc = ((uint16_t)current_command[138]) << 8;
		psc += (uint16_t)current_command[137];
		if(current_command[146] == HK_REPORT)
		{
			hk_telem_count++;
			packetize_send_telemetry(HK_TASK_ID, HK_GROUND_ID, HK_SERVICE, HK_REPORT, hk_telem_count, 1, current_command);
		}
		if(current_command[146] == HK_DEFINITON_REPORT)
		{
			hk_def_report_count++;
			packetize_send_telemetry(HK_TASK_ID, HK_GROUND_ID, HK_SERVICE, HK_DEFINITON_REPORT, hk_def_report_count, 1, current_command);
		}
		if(current_command[146] == TASK_TO_OPR_TCV)
		{
			send_tc_verification(packet_id, psc, current_command[145], current_command[144], 0, 2);		// Verify execution completion.
		}
	}
	if(xQueueReceive(time_to_obc_fifo, current_command, (TickType_t)1) == pdTRUE)
	{
		packet_id = ((uint16_t)current_command[6]) << 8;
		packet_id += (uint16_t)current_command[5];
		psc = ((uint16_t)current_command[4]) << 8;
		psc += (uint16_t)current_command[3];
		if(current_command[9] == TIME_REPORT)
		{
			time_report_count++;
			packetize_send_telemetry(TIME_TASK_ID, TIME_GROUND_ID, TIME_SERVICE, TIME_REPORT, time_report_count, 1, current_command);
		}
		if(current_command[9] == TASK_TO_OPR_TCV)
		send_tc_verification(packet_id, psc, current_command[8], current_command[7], 0, 2);
	}
	if(xQueueReceive(mem_to_obc_fifo, current_command, (TickType_t)1) == pdTRUE)
	{
		packet_id = ((uint16_t)current_command[140]) << 8;
		packet_id += (uint16_t)current_command[139];
		psc = ((uint16_t)current_command[138]) << 8;
		psc += (uint16_t)current_command[137];
		//if(current_command[146] == MEMORY_DUMP_ABS)
		//{
			//mem_dump_count++;
			//packetize_send_telemetry(MEMORY_TASK_ID, MEM_GROUND_ID, MEMORY_SERVICE, MEMORY_DUMP_ABS, mem_dump_count, current_command[145], current_command);
		//}
		//if(current_command[146] == TASK_TO_OPR_TCV)
			//send_tc_verification(packet_id, psc, current_command[145], current_command[144], 0, 2);
		//if(current_command[146] == MEMORY_CHECK_ABS)
		//{
			//mem_check_count++;
			//packetize_send_telemetry(MEMORY_TASK_ID, MEM_GROUND_ID, MEMORY_SERVICE, MEMORY_CHECK_ABS, mem_check_count, 1, current_command);
		//}
		//if(current_command[146] == TASK_TO_OPR_EVENT)
		//{
			//send_event_packet(MEMORY_TASK_ID, current_command[145]);
		//}
		if(current_command[146] == DOWNLINKING_SCIENCE)
		{
			science_packet_count++;
			packetize_send_telemetry(MEMORY_TASK_ID, MEM_GROUND_ID, MEMORY_SERVICE, DOWNLINKING_SCIENCE, science_packet_count, 1, current_command);
		}
	}
	//if(xQueueReceive(sched_to_obc_fifo, current_command, (TickType_t)1) == pdTRUE)
	//{
		//packet_id = ((uint16_t)current_command[140]) << 8;
		//packet_id += (uint16_t)current_command[139];
		//psc = ((uint16_t)current_command[138]) << 8;
		//psc += (uint16_t)current_command[137];
		//if(current_command[146] == SCHED_REPORT)
		//{
			//sched_report_count++;
			//packetize_send_telemetry(SCHEDULING_TASK_ID, SCHED_GROUND_ID, K_SERVICE, SCHED_REPORT, sched_report_count, current_command[145], current_command);
		//}
		//if(current_command[146] == TASK_TO_OPR_TCV)
			//send_tc_verification(packet_id, psc, current_command[145], current_command[144], 0, 2);
		//if(current_command[146] == TASK_TO_OPR_EVENT)
		//{
			//send_event_packet(SCHEDULING_TASK_ID, current_command[145]);
		//}
		//if(current_command[146] == COMPLETED_SCHED_COM_REPORT)
		//{
			//sched_command_count++;
			//packetize_send_telemetry(SCHEDULING_TASK_ID, SCHED_GROUND_ID, K_SERVICE, COMPLETED_SCHED_COM_REPORT, sched_command_count, current_command[145], current_command);
		//}
	//}
	//if(xQueueReceive(event_msg_fifo, &low, (TickType_t)1) == pdTRUE)
	//{
		//xQueueReceive(event_msg_fifo, &high, (TickType_t)1);
		//send_event_packet(high, low);
	//}
	//
	//if(xQueueReceive(fdir_to_obc_fifo, current_command, (TickType_t)1) == pdTRUE)
	//{
		//packet_id = ((uint16_t)current_command[140]) << 8;
		//packet_id += (uint16_t)current_command[139];
		//psc = ((uint16_t)current_command[138]) << 8;
		//psc += (uint16_t)current_command[137];
		//if(current_command[146] == TASK_TO_OPR_TCV)
			//send_tc_verification(packet_id, psc, current_command[145], current_command[144], 0, 2);
		//if(current_command[146] == TASK_TO_OPR_EVENT)
		//{
			//send_event_packet(FDIR_TASK_ID, current_command[145]);
		//}
		////diagnostics reports
		////send diagnostics reports to the housekeeping ground service
		//if (current_command[146] == DIAG_REPORT)
		//{
			//diag_telem_count++;
			//packetize_send_telemetry(FDIR_TASK_ID, HK_GROUND_ID, HK_SERVICE, DIAG_REPORT, diag_telem_count, 1, current_command);
		//}
		//if (current_command[146] == DIAG_DEFINITION_REPORT)
		//{
			//diag_def_report_count++;
			//packetize_send_telemetry(FDIR_TASK_ID, HK_GROUND_ID, HK_SERVICE, DIAG_DEFINITION_REPORT, diag_def_report_count, 1, current_command);
		//}
		//if (current_command[146] == SINGLE_PARAMETER_REPORT)
		//{
			//packetize_send_telemetry(OBC_PACKET_ROUTER_ID, GROUND_PACKET_ROUTER_ID, K_SERVICE, SINGLE_PARAMETER_REPORT, sin_par_rep_count++, 1, current_command);
		//}
	//}
	//if(xQueueReceive(eps_to_obc_fifo, current_command, (TickType_t)1) == pdTRUE)
	//{
		//if(current_command[146] == TASK_TO_OPR_EVENT)
		//{
			//send_event_packet(EPS_TASK_ID, current_command[145]);
		//}
	//}
	return;
}

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
	packet_error_control = 0;
	abs_time = ((uint16_t)absolute_time_arr[0]) << 12;	// DAY
	abs_time = ((uint16_t)absolute_time_arr[1]) << 8;	// HOUR
	abs_time = ((uint16_t)absolute_time_arr[2]) << 4;	// MINUTE
	abs_time = (uint16_t)absolute_time_arr[3];			// SECOND
	
	if(current_tm_fullf)		// Current_tm should not normally be full.
		return -1;
	
	if(num_packets > 1)
		sequence_flags = 0x1;	// Indicates that this is the first packet in a series of packets.
	else
		sequence_flags = 0x3;	// Indicates that this is a standalone packet.
	// Packet Header
	version = 0;
	current_tm[151] = ((version & 0x07) << 5) | ((type & 0x01) << 4) | (0x08);
	current_tm[150] = sender;
	current_tm[149] = (sequence_flags & 0x03) << 6;
	current_tm[148] = sequence_count;
	current_tm[147] = 0x00;
	current_tm[146]	= PACKET_LENGTH - 1;	// Represents the length of the data field - 1.
	version = 1;
	// Data Field Header
	current_tm[145] = (version & 0x07) << 4 | 0x80;
	current_tm[144] = service_type;
	current_tm[143] = service_sub_type;
	current_tm[142] = packet_sub_counter;
	current_tm[141] = dest;
	current_tm[140] = (uint8_t)((abs_time & 0xFF00) >> 8);
	current_tm[139] = (uint8_t)(abs_time & 0x00FF);
	// The Packet Error Control (PEC) is put at the end of the packet.
	pec = fletcher16(current_tm + 2, 150);
	current_tm[1] = (uint8_t)((pec & 0xFF00) >> 8);
	current_tm[0] = (uint8_t)(pec & 0x00FF);

	if(num_packets == 1)
	{
		sequence_count++;
		for(j = 2; j < 130; j++)
		{
			current_tm[j] = *(data + (j - 2));
		}
		
		/* Run checksum on the PUS packet	*/
		packet_error_control = fletcher16(current_tm + 2, 150);
		current_tm[1] = (uint8_t)(packet_error_control >> 8);
		current_tm[0] = (uint8_t)(packet_error_control & 0x00FF);
		current_tm_fullf = 1;
		
		if(store_current_tm() < 0)
			return -1;
		
		return 1;
	}
	
	for(i = 0; i < num_packets; i++)
	{
		current_tm[148] = sequence_count;
		sequence_count++;
		
		if(i > 1)
			sequence_flags = 0x0;			// Continuation packet
		if(i == (num_packets - 1))
			sequence_flags = 0x2;			// Last packet
		current_tm[149] = (sequence_flags & 0x03) << 6;
		
		for(j = 2; j < (PACKET_LENGTH - 13); j++)
		{
			current_tm[j] = *(data + (j - 2) + (i * 128));
		}
		
		/* Run checksum on the PUS packet	*/
		packet_error_control = fletcher16(current_tm + 2, 150);
		current_tm[1] = (uint8_t)(packet_error_control >> 8);
		current_tm[0] = (uint8_t)(packet_error_control & 0x00FF);
		current_tm_fullf = 1;
		if(store_current_tm() < 0)
			return i;
	}
	
	return num_packets;
}

/************************************************************************/
/* RECEIVE_TC_MSG		                                                */
/* @Purpose: Telecommands are broken up into 4 byte messages which are	*/
/* sent by the SSM. Some sequence control is involved. As such, this	*/
/* function is in charge of checking whether the sequence counter is	*/
/* okay and if it is, placing the 4 bytes in current_tc[]				*/
/* @return: -1 : something went wrong or the received sequence was wrong*/
/************************************************************************/
static int receive_tc_msg(void)
{
	uint8_t ssm_seq_count = (uint8_t)(new_tc_msg_high & 0x000000FF);
	low_received = 0;
	high_received = 0;
	
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
		current_tc[(ssm_seq_count * 4) + 1] = (uint8_t)((new_tc_msg_low & 0x0000FF00) >> 8);
		current_tc[(ssm_seq_count * 4) + 2] = (uint8_t)((new_tc_msg_low & 0x00FF0000) >> 16);
		current_tc[(ssm_seq_count * 4) + 3] = (uint8_t)((new_tc_msg_low & 0xFF000000) >> 24);
		if(ssm_seq_count == (PACKET_LENGTH / 4) - 1)
		{
			tc_sequence_count = 0;
			receiving_tcf = 0;
			current_tc_fullf = 1;
			send_tc_transaction_response(ssm_seq_count);
			send_tc_transaction_response(ssm_seq_count);
			store_current_tc();
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

/************************************************************************/
/* SEND_PUS_PACKET_TM	                                                */
/* @Purpose: This function breaks down the PUS packet into multiple CAN	*/
/* messages that are sent in turn and then placed into a telemetry		*/
/* buffer on the side of the SSM.										*/
/* @Note: It is assumed that the PUS packet shall be located in			*/
/* tm_to_downlink[] which is defined in this file						*/
/* @Note: Instead on putting time in Byte 4, I'm going to use it for	*/
/* sequence control so that the SSM can check to make sure that no		*/
/* chunks of the packet were lost.										*/
/* @Note: It should also be obvious that this packet is being sent to	*/
/* the COMS SSM.														*/
/* @Note: DO NOT call this function from an ISR.						*/
/************************************************************************/
static int send_pus_packet_tm(uint8_t sender_id)
{
	uint32_t i;
	num_transfers = PACKET_LENGTH / 4;
	timeout = 500;
	xTimeToWait = 25;
	
	tm_transfer_completef = 0;
	start_tm_transferf = 0;
	send_tc_can_command(0x00, 0x00, sender_id, COMS_ID, TM_PACKET_READY, COMMAND_PRIO);	// Let the SSM know that a TM packet is ready.
	while(!start_tm_transferf)					// Wait for ~25 ms, for the SSM to say that we're good to start/
	{
		if(!timeout--)
		{
			return -1;
		}
		send_tc_can_command(0x00, 0x00, sender_id, COMS_ID, TM_PACKET_READY, COMMAND_PRIO);	// Let the SSM know that a TM packet is ready.
		taskYIELD();
	}
	start_tm_transferf = 0;
	
	for(i = 0; i < num_transfers; i++)
	{
		if(tm_transfer_completef == 0xFF)			// The transaction has failed.
			return -1;
		low =	(uint32_t)tm_to_downlink[(i * 4)];			// Place the data into the lower 4 bytes of the CAN message.
		low += (uint32_t)(tm_to_downlink[(i * 4) + 1] << 8);
		low += (uint32_t)(tm_to_downlink[(i * 4) + 2] << 16);
		low += (uint32_t)(tm_to_downlink[(i * 4) + 3] << 24);
		send_tc_can_command(low, i, sender_id, COMS_ID, SEND_TM, COMMAND_PRIO);
		xLastWakeTime = xTaskGetTickCount();		// Causes a mandatory delay of at least 100ms (10 * 1ms)
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
	}
	timeout = 500;
	while(!tm_transfer_completef)					// Delay for ~100 ms for the SSM to let the OBC know that
	{												// the transfer has completed.
		if(!timeout--)
		{
			return -1;			
		}
		taskYIELD();
	}
	
	if(tm_transfer_completef != (PACKET_LENGTH / 4) - 1)
	{
		tm_transfer_completef = 0;
		return -1;
	}
	else
	{
		tm_transfer_completef = 1;
		tm_down_fullf = 0;
		return tm_transfer_completef;
	}
}

/************************************************************************/
/* SEND_TC_TRANSACTION_RESPONSE	                                        */
/* @Purpose: This function takes in a code of whether or not the		*/
/* incoming TC chunk was okay, and then sends a CAN message to the SSM	*/
/* to let it know.														*/
/* @param: code: The status of the transac response.					*/
/************************************************************************/
static void send_tc_transaction_response(uint8_t code)
{
	uint32_t low;
	low = (uint32_t)code;	
	send_tc_can_command(low, CURRENT_MINUTE, OBC_PACKET_ROUTER_ID, COMS_ID, TC_TRANSACTION_RESP, COMMAND_PRIO);
	return;
}

/************************************************************************/
/* CLEAR_CURRENT_TC		                                                */
/* @Purpose: clears the array current_tc[]								*/
/************************************************************************/
static void clear_current_tc(void)
{
	uint8_t i;
	for(i = 0; i < PACKET_LENGTH; i++)
	{
		current_tc[i] = 0;
	}
	return;
}

/************************************************************************/
/* CLEAR_CURRENT_DATA		                                            */
/* @Purpose: clears the array current_data[]							*/
/************************************************************************/
static void clear_current_data(void)
{
	uint8_t i;
	for(i = 0; i < DATA_LENGTH; i++)
	{
		current_data[i] = 0;
	}
	return;
}

/************************************************************************/
/* CLEAR_CURRENT_COMMAND	                                            */
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
/* STORE_CURRENT_TC			                                            */
/* @Purpose: copies the contents of current_tc[] into the tc_buffer		*/
/************************************************************************/
static int store_current_tc(void)
{
	//uint8_t i, test = 0;
	//if(TC_PACKET_COUNT == MAX_TM_PACKETS)
		//return -1;
	//if(TC_PACKET_COUNT == MAX_TM_PACKETS / 2)
		//send_event_report(1, TC_BUFFER_HALF_FULL, 0, 0);
	//for(i = 0; i < 152; i++)
	//{
		//test = current_tc[i];
	//}
	//
	//if(spimem_write(CURRENT_TC_PACKET, current_tc, 152) != 152)
		//return -1;
		//
	//TC_PACKET_COUNT++;
	//spimem_write(TC_BASE, &TC_PACKET_COUNT, 4);
//
	//CURRENT_TC_PACKET += 152;
	//if(CURRENT_TC_PACKET > (TC_BASE + 0x20000))
		//CURRENT_TC_PACKET = TC_BASE + 12;
	//spimem_write(TC_BASE + 8, &CURRENT_TC_PACKET, 4);
	if(xQueueSendToBack(tc_buffer, current_tc, (TickType_t)1) != pdPASS)
	{
		send_event_report(1, TC_BUFFER_FULL, 0, 0);		// FAILURE_RECOVERY
		return -1;
	}
	current_tc_fullf = 0;
	return 1;
}

/************************************************************************/
/* STORE_CURRENT_TM			                                            */
/* @Purpose: copies the contents of current_tc[] into the tc_buffer		*/
/************************************************************************/
static int store_current_tm(void)
{
	//if(TM_PACKET_COUNT == MAX_TM_PACKETS)
		//return -1;
	//if(TM_PACKET_COUNT == MAX_TM_PACKETS / 2)
		//send_event_report(1, TM_BUFFER_HALF_FULL, 0, 0);
		//
	//TM_PACKET_COUNT++;
	//spimem_write(TM_BASE, &TM_PACKET_COUNT, 4);		// FAILURE_RECOVERY
	//
	//CURRENT_TM_PACKET += 152;
	//if(CURRENT_TM_PACKET > (TM_BASE + 0x20000))
		//CURRENT_TM_PACKET = TM_BASE + 12;
	//spimem_write(TM_BASE + 8, &CURRENT_TM_PACKET, 4);		// FAILURE_RECOVERY
//
	//if(spimem_write(CURRENT_TM_PACKET, current_tm, 152) < 0)					// FAILURE_RECOVERY
		//return -1;
	//current_tm_fullf = 0;
	if(xQueueSendToBack(tm_buffer, current_tm, (TickType_t)1) != pdPASS)
	{
		return -1;										// FAILURE_RECOVERY
	}
	current_tm_fullf = 0;
	return 1;
}



/************************************************************************/
/* DECODE_TELECOMMAND		                                            */
/* @Purpose: This function is meant to parse through the telecommand	*/
/* contained in tc_to_decode[] and then either perform an action or		*/
/* route a message to a task or send a CAN message to an SSM			*/
/* @Note: This function assumes that the telecommand to decode is		*/
/* contained in tc_to_decode[]											*/
/************************************************************************/
static int decode_telecommand(void)
{
	uint8_t i; //, test = 0;
	int x, attempts;
	
	packet_id = (uint16_t)(tc_to_decode[151]);
	packet_id = packet_id << 8;
	packet_id |= (uint16_t)(tc_to_decode[150]);
	psc = (uint16_t)(tc_to_decode[149]);
	psc = psc << 8;
	psc |= (uint16_t)(tc_to_decode[148]);
	
	// PACKET HEADER
	version1			= (tc_to_decode[151] & 0xE0) >> 5;
	type1				= (tc_to_decode[151] & 0x10) >> 4;
	data_field_headerf	= (tc_to_decode[151] & 0x08) >> 3;
	apid				= tc_to_decode[150];
	sequence_flags1		= (tc_to_decode[149] & 0xC0) >> 6;
	sequence_count1		= tc_to_decode[148];
	packet_length		= tc_to_decode[146] + 1;				// B137 = PACKET_LENGTH - 1
	// DATA FIELD HEADER
	ccsds_flag			= (tc_to_decode[145] & 0X80) >> 7;
	packet_version		= (tc_to_decode[145] & 0X70) >> 4;
	ack					= tc_to_decode[145] & 0X0F;
	service_type		= tc_to_decode[144];
	service_sub_type	= tc_to_decode[143];
	source_id			= tc_to_decode[142];
	
	pec1 = (uint16_t)(tc_to_decode[1]);
	pec1 = pec1 << 8;
	pec1 += (uint16_t)(tc_to_decode[0]);
	
	//for(i = 0; i < PACKET_LENGTH; i++)
	//{
		//test = tc_to_decode[i];
	//}
	
	/* Check that the packet error control is correct		*/
	pec0 = fletcher16(tc_to_decode + 2, 150);
	/* Verify that the telecommand is ready to be decoded.	*/
	
	//attempts = 0; x = -1;
	//while (attempts<3 && x<0){
		x = verify_telecommand(apid, packet_length, pec0, pec1, service_type, service_sub_type, version1, ccsds_flag, packet_version);		// FAILURE_RECOVERY required if x == -1.
		//attempts++;
	//}
	//if(x < 0)
	//{	
		//errorREPORT(OBC_ID, service_type, OBC_TC_PACKET_ERROR, 0);
		//return -1;
	//}
	/* Decode the telecommand packet						*/		// To be updated on a rolling basis
	return decode_telecommand_h(service_type, service_sub_type);
}

/************************************************************************/
/* DECODE_TELECOMMAND_H		                                            */
/* @Purpose: helper to decode_telecommand, this function looks at the	*/
/* service_type and service_sub_type of the telecommand packet stored in*/
/* tc_to_decode[] and performs the actual routing of messages and		*/
/* executing of required actions.										*/
/* @param: service_type: ex: Housekeeping = 3.							*/
/* @param: service_sub_type: ex: TC Verification, success == 1			*/
/************************************************************************/
static int decode_telecommand_h(uint8_t service_type, uint8_t service_sub_type)
{	
	sID = 0xFF;
	collection_interval = 0;
	npar1 = 0;
	uint8_t i; //severity=0;
	uint32_t val = 0;
	uint32_t time = 0;
	int status = 0;
	clear_current_command();
	for(i = 0; i < DATA_LENGTH; i++)
	{
		current_command[i] = tc_to_decode[i + 2];
	}
	
	current_command[140] = ((uint8_t)packet_id) >> 8;	// Place packet_id and psc inside command in case a TC verification is needed.
	current_command[139] = (uint8_t)packet_id;
	current_command[138] = ((uint8_t)psc) >> 8;
	current_command[137] = (uint8_t)psc;
	
	if(service_type == HK_SERVICE)
	{
		switch(service_sub_type)
		{
			case	NEW_HK_DEFINITION:
				sID = current_command[136];			// Structure ID for this definition.
				if(sID != 1)
				{
					send_tc_verification(packet_id, psc, 0xFF, 5, 0x00, 1);				// Only sID of 1 is allowed.
					return -1;
				}
				collection_interval = (uint32_t)current_command[135];	
				npar1 = current_command[134];
				if(npar1 > 64)
				{
					send_tc_verification(packet_id, psc, 0xFF, 5, 0x00, 1);				// Npar1 must be <= 64
					return -1;
				}
				current_command[146] = NEW_HK_DEFINITION;
				current_command[145] = collection_interval;
				current_command[144] = npar1;
				xQueueSendToBack(obc_to_hk_fifo, current_command, (TickType_t)1);		// FAILURE_RECOVERY if this doesn't return pdTrue.
				break;
			case	CLEAR_HK_DEFINITION:
				sID = current_command[136];
				if(sID != 1)
				{
					send_tc_verification(packet_id, psc, 0xFF, 5, 0x00, 1);				// Usage error.
					return -1;
				}
				current_command[146] = CLEAR_HK_DEFINITION;
				xQueueSendToBack(obc_to_hk_fifo, current_command, (TickType_t)1);
				break;
			case	ENABLE_PARAM_REPORT:
				current_command[146] = ENABLE_PARAM_REPORT;
				xQueueSendToBack(obc_to_hk_fifo, current_command, (TickType_t)1);
				break;
			case	DISABLE_PARAM_REPORT:
				current_command[146] = DISABLE_PARAM_REPORT;
				xQueueSendToBack(obc_to_hk_fifo, current_command, (TickType_t)1);
				break;
			case	REPORT_HK_DEFINITIONS:
				current_command[146] = REPORT_HK_DEFINITIONS;
				xQueueSendToBack(obc_to_hk_fifo, current_command, (TickType_t)1);
				break;
				
			// William: Put more diagnostics stuff here. This time I want the service_type in current_command[146] and
			// The service_sub_type placed in current_command[145]
			case	NEW_DIAG_DEFINITION:
				current_command[146] = HK_SERVICE;
				current_command[145] = NEW_DIAG_DEFINITION;
				xQueueSendToBack(obc_to_fdir_fifo, current_command, (TickType_t)1);
				break;
			
			case	CLEAR_DIAG_DEFINITION:
				current_command[146] = HK_SERVICE;
				current_command[145] = CLEAR_DIAG_DEFINITION;
				xQueueSendToBack(obc_to_fdir_fifo, current_command, (TickType_t)1);
				break;
			
			case	ENABLE_D_PARAM_REPORT:
				current_command[146] = HK_SERVICE;
				current_command[145] = ENABLE_D_PARAM_REPORT;
				xQueueSendToBack(obc_to_fdir_fifo, current_command, (TickType_t)1);
				break;
			case	DISABLE_D_PARAM_REPORT:
				current_command[146] = HK_SERVICE;
				current_command[145] = DISABLE_D_PARAM_REPORT;
				xQueueSendToBack(obc_to_fdir_fifo, current_command, (TickType_t)1);
				break;
			case	REPORT_DIAG_DEFINITIONS:
				current_command[146] = HK_SERVICE;
				current_command[145] = REPORT_DIAG_DEFINITIONS;
				xQueueSendToBack(obc_to_fdir_fifo, current_command, (TickType_t)1);
				break;
			default:
				return -1;
		}
		return 1;
	}
	if(service_type == TIME_SERVICE)
	{
		current_command[9] = UPDATE_REPORT_FREQ;
		current_command[8] = ((uint8_t)packet_id) >> 8;	// Place packet_id and psc inside command in case a TC verification is needed.
		current_command[7] = (uint8_t)packet_id;
		current_command[6] = ((uint8_t)psc) >> 8;
		current_command[5] = (uint8_t)psc;
		current_command[0] = current_command[0];			// Report Freq.
		xQueueSendToBack(obc_to_time_fifo, current_command, (TickType_t)1);
		return 1;
	}
	if(service_type == MEMORY_SERVICE)
	{
		current_command[146] = service_sub_type;
		current_command[140] = (uint8_t)(packet_id >> 8);
		current_command[139] = (uint8_t)(packet_id & 0x00FF);
		current_command[138] = (uint8_t)(psc >> 8);
		current_command[137] = (uint8_t)(psc & 0x00FF);
		if(!SAFE_MODE)
			xQueueSendToBack(obc_to_mem_fifo, current_command, (TickType_t)1);
		if(SAFE_MODE)
			xQueueSendToBack(obc_to_fdir_fifo, current_command, (TickType_t)1);
		return 1;
	}
	if(service_type == K_SERVICE)
	{
		if(service_sub_type == ADD_SCHEDULE)
		{
			//severity = current_command[129];
			time = ((uint32_t)current_command[135]) << 24;
			time += ((uint32_t)current_command[134]) << 16;
			time += ((uint32_t)current_command[133]) << 8;
			time += (uint32_t)current_command[132];
			current_command[146] = service_sub_type;
			xQueueSendToBack(obc_to_sched_fifo, current_command, (TickType_t)1);	// All scheduled commands should be sent to scheduling.		
		}
		if(service_sub_type == START_EXPERIMENT_ARM)
		{
			experiment_armed = 1;
			send_tc_verification(packet_id, psc, 0, OBC_PACKET_ROUTER_ID, 0, 2);	// Successful command execution report.
		}
		if(service_sub_type == START_EXPERIMENT_FIRE)
		{
			if(experiment_armed)
			{
				experiment_started = 1;
				send_tc_verification(packet_id, psc, 0, OBC_PACKET_ROUTER_ID, 0, 2);	// Successful command execution report.
			}
			else
				send_tc_verification(packet_id, psc, 0xFF, 5, 0, 1);				// Failed telecommand acceptance report (usage error due to experiment_armed = 0)
		}
		if(service_sub_type == SET_VARIABLE)
		{
			ssmID = get_ssm_id(current_command[136]);
			val = (uint32_t)current_command[132];
			val += ((uint32_t)current_command[133]) << 8;
			val += ((uint32_t)current_command[134]) << 16;
			val += ((uint32_t)current_command[135]) << 24;			
			if(ssmID < 3)
				set_variable(OBC_PACKET_ROUTER_ID, ssmID, current_command[136], (uint16_t)val);
			else
				set_obc_variable(current_command[136], val);
			send_tc_verification(packet_id, psc, 0, OBC_PACKET_ROUTER_ID, 0, 2);
		}
		if(service_sub_type == GET_PARAMETER)
		{
			ssmID = get_ssm_id(current_command[136]);
			val = (uint32_t)current_command[132];
			val += ((uint32_t)current_command[133]) << 8;
			val += ((uint32_t)current_command[134]) << 16;
			val += ((uint32_t)current_command[135]) << 24;
			if(ssmID < 3)
				val = request_sensor_data(OBC_PACKET_ROUTER_ID, ssmID, current_command[136], &status);
			else
				val = get_obc_variable(current_command[136]);
			send_tc_verification(packet_id, psc, 0, OBC_PACKET_ROUTER_ID, 0, 2);
			i = current_command[136];
			clear_current_command();
			current_command[136] = i;
			current_command[132] = (uint8_t)val;
			current_command[133] = (uint8_t)(val >> 8);
			current_command[134] = (uint8_t)(val >> 16);
			current_command[135] = (uint8_t)(val >> 24);			
			packetize_send_telemetry(OBC_PACKET_ROUTER_ID, GROUND_PACKET_ROUTER_ID, K_SERVICE, SINGLE_PARAMETER_REPORT, sin_par_rep_count++, 1, current_command);
		}
		if(service_sub_type == DEPLOY_ANTENNA)
		{
			send_can_command(0, 0, OBC_PACKET_ROUTER_ID, EPS_ID, DEP_ANT_COMMAND, DEF_PRIO);
			send_tc_verification(packet_id, psc, 0, OBC_PACKET_ROUTER_ID, 0, 2);	// Let ground know the command was executed.
			time_of_deploy = xTaskGetTickCount();
			deployed_antenna = 1;
		}
		else
		{
			/* Everything else should be sent to the scheduling task. */
			current_command[146] = service_sub_type;
			xQueueSendToBack(obc_to_sched_fifo, current_command, (TickType_t)1);			
		}
	}
	if(service_type == FDIR_SERVICE)
	{
		current_command[146] = service_type;
		current_command[145] = service_sub_type;
		if((service_sub_type == PAUSE_SSM_OPERATIONS) || (service_sub_type == RESUME_SSM_OPERATIONS) || (service_sub_type == RESET_SSM) 
					|| (service_sub_type == REPROGRAM_SSM) || (service_sub_type == RESET_TASK))
			current_command[144] = current_command[129];
		xQueueSendToBack(obc_to_fdir_fifo, current_command, (TickType_t)1);
	}
	
	return 1;
}

/************************************************************************/
/* VERIFY_TELECOMMAND		                                            */
/* @Purpose: This helper is used to determine whether or not the		*/
/* received TC packet is valid for decoding								*/
/* @param: apid: The process id of that the TC packet is meant for.		*/
/* @param: packet_length: length of the packet in bytes.				*/
/* @param: pec0: The checksum contained in the telecommand packet		*/
/* @param: pec1: The checksum which was computed by this OBC			*/
/* @param: service_type: ex: Housekeeping = 3.							*/
/* @param: service_sub_type: ex: TC Verification, success == 1			*/
/* @param: version: PUS version as indicated by the TC packet.			*/
/* @param: ccsds_flag: does packet contain a PUS standard data header?	*/
/* @param: packet_version: PUS version within the data header.			*/
/* @return: -1: TC packet failed inspection, 1 == Good to decode		*/
/************************************************************************/
static int verify_telecommand(uint8_t apid, uint8_t packet_length, uint16_t pec0, uint16_t pec1, uint8_t service_type, uint8_t service_sub_type, uint8_t version, uint8_t ccsds_flag, uint8_t packet_version)
{
	address = 0;
	length = 0;
	uint8_t i;
	new_time = 0;
	last_time = 0;
	if(packet_length != PACKET_LENGTH)
	{
		send_tc_verification(packet_id, psc, 0xFF, 1, (uint32_t)packet_length, 1);		// TC verify acceptance report, failure, 1 == invalid packet length
		return -1;
	}
	
	if(pec0 != pec1)
	{
		send_tc_verification(packet_id, psc, 0xFF, 2, (uint32_t)pec1, 1);				// TC verify acceptance report, failure, 2 == invalid PEC (checksum)
		return -1;
	}
	
	if((service_type != 3) && (service_type != 6) && (service_type != 9) && (service_type != 69))
	{
		send_tc_verification(packet_id, psc, 0xFF, 3, (uint32_t)service_type, 1);		// TC verify acceptance report, failure, 3 == invalid service type
		return -1;
	}
	
	if(service_type == HK_SERVICE)
	{
		if((service_sub_type != 1) && (service_sub_type != 2) && (service_sub_type != 3) && (service_sub_type != 4) && (service_sub_type != 5) && (service_sub_type != 6)
		&& (service_sub_type != 7) && (service_sub_type != 8) && (service_sub_type != 9) && (service_sub_type != 11) && (service_sub_type != 17) && (service_sub_type != 18))
		{
			//send_tc_verification(packet_id, psc, 0xFF, 4, (uint32_t)service_sub_type, 1);	// TC verify acceptance report, failure, 4 == invalid service subtype.
			return -1;
		}
		
		if((apid != HK_TASK_ID) && (apid != FDIR_TASK_ID))
		{
			//send_tc_verification(packet_id, psc, 0xFF, 0, (uint32_t)apid, 1);				// TC verify acceptance report, failure, 0 == invalid apid
			return -1;
		}
	}
	
	if(service_type == MEMORY_SERVICE)
	{
		if((service_sub_type != 2) && (service_sub_type != 5) && (service_sub_type != 9))
		{
			send_tc_verification(packet_id, psc, 0xFF, 4, (uint32_t)service_sub_type, 1);	// TC verify acceptance report, failure, 4 == invalid service subtype
			return -1;
		}
		if(apid != MEMORY_TASK_ID)
		{
			send_tc_verification(packet_id, psc, 0xFF, 0, (uint32_t)apid, 1);				// TC verify acceptance report, failure, 0 == invalid apid
			return -1;
		}
		address =  ((uint32_t)tc_to_decode[137]) << 24;
		address += ((uint32_t)tc_to_decode[136]) << 16;
		address += ((uint32_t)tc_to_decode[135]) << 8;
		address += (uint32_t)tc_to_decode[134];
		
		if(tc_to_decode[138] > 1)											// Invalid memory ID.
			send_tc_verification(packet_id, psc, 0xFF, 5, 0x00, 1);
		if((tc_to_decode[138] == 1) && (address > 0xFFFFF))				// Invalid memory address (too high)
			send_tc_verification(packet_id, psc, 0xFF, 5, 0x00, 1);
		if((tc_to_decode[138] == 1) && INTERNAL_MEMORY_FALLBACK_MODE && (address > 0x0FFF))		// Invalid memory address (too high for INT MEM FALLBACK MODE)
			send_tc_verification(packet_id, psc, 0xFF, 5, 0x00, 1);			
	}
	
	if(service_type == TIME_SERVICE)
	{
		if(service_sub_type != UPDATE_REPORT_FREQ)
		{
			send_tc_verification(packet_id, psc, 0xFF, 4, (uint32_t)service_sub_type, 1);		// TC verify acceptance report, failure, 4 == invalid service subtype
			return -1;			
		}
		if(apid != TIME_TASK_ID)
		{
			send_tc_verification(packet_id, psc, 0xFF, 0, (uint32_t)apid, 1);				// TC verify acceptance report, failure, 0 == invalid apid
			return -1;
		}
	}
	
	if(service_type == K_SERVICE)
	{
		length = tc_to_decode[136];
		
		if((service_sub_type > 13) || !service_sub_type)
		{
			send_tc_verification(packet_id, psc, 0xFF, 4, (uint32_t)service_sub_type, 1);
			return -1;
		}
		if((tc_to_decode[135] || tc_to_decode[134] || tc_to_decode[133] || tc_to_decode[132]) && (service_sub_type != 1))	// Time should be zero for immediate commands
		{
			send_tc_verification(packet_id, psc, 0xFF, 5, 0x00, 1);					// Usage error.
			return -1;
		}
		if(tc_to_decode[135] || tc_to_decode[134] || tc_to_decode[133] || tc_to_decode[132])
		{
			for(i = 0; i < length; i++)
			{
				new_time = ((uint32_t)tc_to_decode[135 - (i * 8)]) << 24;
				new_time += ((uint32_t)tc_to_decode[134 - (i * 8)]) << 16;
				new_time += ((uint32_t)tc_to_decode[133 - (i * 8)]) << 8;
				new_time += (uint32_t)tc_to_decode[132 - (i * 8)];
				if(new_time < last_time)												// Scheduled commands should be in chronological order
				{
					send_tc_verification(packet_id, psc, 0xFF, 5, 0x00, 1);			// Usage error.
					return -1;
				}
				last_time = new_time;
			}
		}
	}
	if(service_type == FDIR_SERVICE)
	{
		if((service_sub_type > 12) || !service_sub_type)
		{
			send_tc_verification(packet_id, psc, 0xFF, 4, (uint32_t)service_sub_type, 1);
			return -1;
		}
	}
	
	if(version != 0)
	{
		send_tc_verification(packet_id, psc, 0xFF, 5, 0x00, 1);							// TC verify acceptance repoort, failure, 5 == usage error
		return -1;
	}
	
	if(ccsds_flag != 1)
	{
		send_tc_verification(packet_id, psc, 0xFF, 5, 0x00, 1);							// TC verify acceptance report, failure, 5 == usage error
		return -1;
	}
	
	if(packet_version != 1)
	{
		send_tc_verification(packet_id, psc, 0xFF, 5, 0x00, 1);							// TC verify acceptance report, failure, 5 == usage error
		return -1;
	}
	
	/* The telecommand packet is good to be decoded further!		*/
	//send_tc_verification(packet_id, psc, 0, 0, 0, 1);										// TC verify acceptance report, success.
	return 1;
}

/************************************************************************/
/* SEND_TC_VERIFICATION		                                            */
/* @Purpose: turns the parameters into a TC verification packet and		*/
/* proceeds to send it to the COMS SSM so that it can be downlinked		*/
/* @param: packet_id: The first 2B of the PUS packet.					*/
/* @param: sequence_control: the next 2B of the PUS packet.				*/
/* @param: status: 0 = SUCCESS, >0 = FAILURE							*/
/* @param: code: For TC_type == 1, code is the error code, for			*/
/* TC_type == 2, code is the APID which completed the command			*/
/* @param: parameter: something informational to send to ground.		*/
/* @param: 1 == acceptance verification, 2 == execution verification	*/
/* @return: -1 = function failed, 1 = function succeeded.				*/
/************************************************************************/
static int send_tc_verification(uint16_t packet_id, uint16_t sequence_control, uint8_t status, uint8_t code, uint32_t parameter, uint8_t tc_type)
{
	int resp = -1;
	clear_current_data();	
	if(tc_type == 1)
	{
		if (code > 5)
			return -1;				// Invalid code inputted.
		if(!status)					// Telecommand Acceptance Report - Success (1,1)
		{
			tc_verify_success_count++;
			current_data[0] = (uint8_t)(sequence_control & 0x00FF);
			current_data[1] = (uint8_t)((sequence_control & 0xFF00) >> 8);
			current_data[2] = (uint8_t)(packet_id & 0x00FF);
			current_data[3] = (uint8_t)((packet_id & 0xFF00) >> 8);
			resp = packetize_send_telemetry(OBC_PACKET_ROUTER_ID, GROUND_PACKET_ROUTER_ID, 1, 1, tc_verify_success_count, 1, current_data);
		}
		else						// Telecommand Acceptance Report - Failure (1,2)
		{
			tc_verify_fail_count++;
			current_data[0] = (uint8_t)((parameter & 0x000000FF));
			current_data[1] = (uint8_t)((parameter & 0x0000FF00) >> 8);
			current_data[2] = (uint8_t)((parameter & 0x00FF0000) >> 16);
			current_data[3] = (uint8_t)((parameter & 0xFF000000) >> 24);
			current_data[4] = code;
			current_data[5] = (uint8_t)(sequence_control & 0x00FF);
			current_data[6] = (uint8_t)((sequence_control & 0xFF00) >> 8);
			current_data[7] = (uint8_t)(packet_id & 0x00FF);
			current_data[8] = (uint8_t)((packet_id & 0xFF00) >> 8);
			resp = packetize_send_telemetry(OBC_PACKET_ROUTER_ID, GROUND_PACKET_ROUTER_ID, 1, 2, tc_verify_success_count, 1, current_data);
		}		
	}
	if(tc_type == 2)
	{
		current_data[0] = (uint8_t)(sequence_control & 0x00FF);
		current_data[1] = (uint8_t)((sequence_control & 0xFF00) >> 8);
		current_data[2] = (uint8_t)(packet_id & 0x00FF);
		current_data[3] = (uint8_t)((packet_id & 0xFF00) >> 8);
		if(status == 0xFF)
		{
			tc_exec_fail_count++;
			resp = packetize_send_telemetry(code, GROUND_PACKET_ROUTER_ID, 1, 8, tc_exec_fail_count, 1, current_data);
		}
		if(status == 1)
		{
			tc_exec_success_count++;
			resp = packetize_send_telemetry(code, GROUND_PACKET_ROUTER_ID, 1, 7, tc_exec_success_count, 1, current_data);
		}
	}
	if(resp == -1)
		return -1;
	else
		return 1;
}

/************************************************************************/
/* SEND_EVENT_PACKET		                                            */
/* @Purpose: Takes information in high & low and turns them into an		*/
/* event report which is then downlinked to the ground station.			*/
/* @param: sender: The ID of the task / SSM which is transmitting this	*/
/* event report.														*/
/* @param: severity: (1|2|3|4), where 1 = NORMAL, 1,2,3 = increasing	*/
/* severity of error.													*/
/* @Note: The task event report function is now responsible for setting */
/* up the data portion of the packet, and hence this function assumes	*/
/* that the array current_data[] has the data necessary for this report	*/
/************************************************************************/
static void send_event_packet(uint8_t sender, uint8_t severity)
{
	event_report_count++;
	packetize_send_telemetry(sender, GROUND_PACKET_ROUTER_ID, 5, severity, event_report_count, 1, current_data);	// FAILURE_RECOVERY
	return;
}

/************************************************************************/
/* SEND_EVENT_REPORT		                                            */
/* @Purpose: sends a message to the OBC_PACKET_ROUTER using				*/
/* sched_to_obc_fifo. The intent is to an event report downlinked to	*/
/* the ground station.													*/
/* @param: severity: 1 = Normal.										*/
/* @param: report_id: Unique to the event report, ex: BIT_FLIP_DETECTED */
/* @param: param1,0 extra information that can be sent to ground.		*/
/************************************************************************/
static void send_event_report(uint8_t severity, uint8_t report_id, uint8_t param1, uint8_t param0)
{
	clear_current_command();
	current_command[136] = report_id;
	current_command[135] = 2;
	current_command[134] = 0x00;
	current_command[133] = 0x00;
	current_command[132] = 0x00;
	current_command[131] = param0;
	current_command[130] = 0x00;
	current_command[129] = 0x00;
	current_command[128] = 0x00;
	current_command[127] = param1;
	send_event_packet(OBC_PACKET_ROUTER_ID, severity);
	return;
}

// This function will kill this task.
// If it is being called by this task 0 is passed, otherwise it is probably the FDIR task and 1 should be passed.
void opr_kill(uint8_t killer)
{
	// Kill the task.
	if(killer)
		vTaskDelete(opr_HANDLE);
	else
		vTaskDelete(NULL);
	return;
}

void set_obc_variable(uint8_t parameter, uint32_t val)
{
	switch(parameter)
	{
		case ABS_TIME_D:
			ABSOLUTE_DAY = (uint8_t)val;
		case ABS_TIME_H:
			CURRENT_HOUR = (uint8_t)val;
		case ABS_TIME_M:
			CURRENT_MINUTE = (uint8_t)val;
		case ABS_TIME_S:
			CURRENT_SECOND = (uint8_t)val;
		case SPI_CHIP_1:
			SPI_HEALTH1 = (uint8_t)val;
		case SPI_CHIP_2:
			SPI_HEALTH2 = (uint8_t)val;
		case SPI_CHIP_3:
			SPI_HEALTH3 = (uint8_t)val;
		case OBC_CTT:
			obc_consec_trans_timeout = val;
		case OBC_OGT:
			obc_ok_go_timeout = val;
		case EPS_BAL_INTV:
			eps_balance_interval = val;
		case EPS_HEAT_INTV:
			eps_heater_interval = val;
		case EPS_TRGT_TMP:
			eps_target_temp = val;
		case EPS_TEMP_INTV:
			eps_temp_interval = val;
		default:
			return;
	}
	return;
}

uint32_t get_obc_variable(uint8_t parameter)
{
	switch(parameter)
	{
		case ABS_TIME_D:
			return (uint32_t)ABSOLUTE_DAY;
		case ABS_TIME_H:
			return (uint32_t)CURRENT_HOUR;
		case ABS_TIME_M:
			return (uint32_t)CURRENT_MINUTE;
		case ABS_TIME_S:
			return (uint32_t)CURRENT_SECOND;
		case SPI_CHIP_1:
			return (uint32_t)SPI_HEALTH1;
		case SPI_CHIP_2:
			return (uint32_t)SPI_HEALTH2;
		case SPI_CHIP_3:
			return (uint32_t)SPI_HEALTH3;
		case OBC_CTT:
			return obc_consec_trans_timeout;
		case OBC_OGT:
			return obc_ok_go_timeout;
		case EPS_BAL_INTV:
			return eps_balance_interval;
		case EPS_HEAT_INTV:
			return eps_heater_interval;
		case EPS_TRGT_TMP:
			return eps_target_temp;
		case EPS_TEMP_INTV:
			return eps_temp_interval;
		default:
			return 0;
	}
	return 0;
}
