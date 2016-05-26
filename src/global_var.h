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
	*	11/26/2015			Adding a couple FIFOs for error reporting / handling.
	*
	*	11/25/2015			Adding some the definitions that have been created since I started
	*						working on the groundstation.
	*
*/

#ifndef GLOBAL_VARH
#define GLOBAL_VARH

#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

/*		PUS DEFINITIONS HERE			*/
#define PACKET_LENGTH	152
#define DATA_LENGTH		137

/* Definitions to clarify which services represent what.		*/
#define TC_VERIFY_SERVICE				1
#define HK_SERVICE						3
#define EVENT_REPORT_SERVICE			5
#define MEMORY_SERVICE					6
#define TIME_SERVICE					9
#define K_SERVICE						69
#define FDIR_SERVICE					70

/* Definitions to clarify which service subtypes represent what	*/
/* Housekeeping							*/
#define NEW_HK_DEFINITION				1
#define CLEAR_HK_DEFINITION				3
#define ENABLE_PARAM_REPORT				5
#define DISABLE_PARAM_REPORT			6
#define REPORT_HK_DEFINITIONS			9
#define HK_DEFINITON_REPORT				10
#define HK_REPORT						25
/* Diagnostics							*/
#define NEW_DIAG_DEFINITION				2
#define CLEAR_DIAG_DEFINITION			4
#define ENABLE_D_PARAM_REPORT			7
#define DISABLE_D_PARAM_REPORT			8
#define REPORT_DIAG_DEFINITIONS			11
#define DIAG_DEFINITION_REPORT			12
#define DIAG_REPORT						26
/* Time									*/
#define UPDATE_REPORT_FREQ				1
#define TIME_REPORT						2
/* Memory Management					*/
#define MEMORY_LOAD_ABS					2
#define DUMP_REQUEST_ABS				5
#define MEMORY_DUMP_ABS					6
#define CHECK_MEM_REQUEST				9
#define MEMORY_CHECK_ABS				10
/* K-Service							*/
#define ADD_SCHEDULE					1
#define CLEAR_SCHEDULE					2
#define	SCHED_REPORT_REQUEST			3
#define SCHED_REPORT					4
#define PAUSE_SCHEDULE					5
#define RESUME_SCHEDULE					6
#define COMPLETED_SCHED_COM_REPORT		7
#define START_EXPERIMENT_ARM			8
#define START_EXPERIMENT_FIRE			9
#define SET_VARIABLE					10
#define GET_PARAMETER					11
#define SINGLE_PARAMETER_REPORT			12
/* FDIR Service							*/
#define ENTER_LOW_POWER_MODE			1
#define EXIT_LOW_POWER_MODE				2
#define ENTER_SAFE_MODE					3
#define EXIT_SAFE_MODE					4
#define ENTER_COMS_TAKEOVER_MODE		5
#define EXIT_COMS_TAKEOVER_MODE			6
#define PAUSE_SSM_OPERATIONS			7
#define RESUME_SSM_OPERATIONS			8
#define REPROGRAM_SSM					9
#define RESET_SSM						10
#define RESET_TASK						11
#define DELETE_TASK						12

/* Action Requests to the OBC_PACKET_ROUTER */
#define TASK_TO_OPR_TCV					0xDD
#define TASK_TO_OPR_EVENT				0xEE
#define DOWNLINKING_SCIENCE				0xCC

/* EVENT REPORT ID						*/
#define KICK_COM_FROM_SCHEDULE			0x01			// A command was kicked from the schedule.
#define BIT_FLIP_DETECTED				0x02			// A bit flip was detected in SPI memory.
#define MEMORY_WASH_FINISHED			0x03			// A memory wash operation completed.
#define ALL_SPIMEM_CHIPS_DEAD			0x04			// All the SPI memory chips are dead.
#define INC_USAGE_OF_DECODE_ERROR		0x05			// Incorrect usafe of decode_error()
#define INTERNAL_MEMORY_FALLBACK		0x06			// OBC has entered into internal memory fallback mode
#define SCHEDULING_MALFUNCTION			0x07			// There was a malfunction in the scheduling task
#define SAFE_MODE_ENTERED				0x08
#define SPI0_MUTEX_MALFUNCTION			0x09
#define SPI_FAILED_IN_FDIR				0x0A			// A SPI memory operation failed during FDIR operations.
#define SCHED_COMMAND_FAILED			0x0B
#define ERROR_IN_RESTART_TASK			0x0C
#define ERROR_IN_RS5					0x0D
#define ERROR_IN_RESET_SSM				0x0E
#define DYSFUNCTIONAL_FIFO				0x0F			// A FIFO write or read failed repetitively
#define FIFO_INFO_LOST					0x10			// A FIFO was recreated and information inside was lost.
#define FIFO_ERROR_WITHIN_FDIR			0x11
#define IMPORTANT_FIFO_FAILED			0x12
#define SPIMEM_ERROR_DURING_INIT		0x13
#define OBC_PARAM_FAILED				0x14
#define REQ_DATA_TIMEOUT_TOO_LONG		0x15
#define ERROR_IN_CFS					0x16
#define SSM_PARAM_FAILED				0x17
#define ER_SEC_TIMEOUT_TOO_LONG			0x18
#define ER_CHIP_TIMEOUT_TOO_LONG		0x19
#define SPIMEM_INIT_FAILED				0x1A
#define ERROR_IN_GFS					0x1B
#define SPIMEM_FAIL_IN_RTC_INIT			0x1C
#define SPIMEM_FAIL_IN_MEM_WASH			0x1D
#define SSM_CTT_TOO_LONG				0x1E
#define OBC_CTT_TOO_LONG				0x1F
#define SAFE_MODE_EXITED				0x20
#define CAN_ERROR_WITHIN_FDIR			0x21
#define ERROR_IN_DELETE_TASK			0x22
#define INTERNAL_MEMORY_FALLBACK_EXITED 0x23
#define DIAG_ERROR_IN_FDIR				0x24
#define DIAG_SPIMEM_ERROR_IN_FDIR		0x25
#define DIAG_SENSOR_ERROR_IN_FDIR		0x26
#define TC_BUFFER_FULL					0x27
#define TM_BUFFER_FULL					0x28
#define EPS_SENSOR_VALUE_OUT_OF_RANGE	0X29
#define BATTERY_HEATER_STATUS			0x2A
#define COMMAND_NOT_SCHEDULABLE			0x2B
#define TM_BUFFER_HALF_FULL				0x2C
#define TC_BUFFER_HALF_FULL				0x2D

/*  CAN GLOBAL FIFOS				*/
/* Initialized in prvInitializeFifos() in main.c	*/
QueueHandle_t can_data_fifo;			// CAN Handler	-->		data_collect
QueueHandle_t can_msg_fifo;				// CAN Handler	-->		data_collect
QueueHandle_t can_hk_fifo;				// CAN Handler	-->		housekeep
QueueHandle_t can_com_fifo;				// CAN Handler	-->		command_test
QueueHandle_t tc_msg_fifo;				// CAN Handler	-->		obc_packet_router
QueueHandle_t event_msg_fifo;			// CAN Handler	-->		obc_packet_router
QueueHandle_t fdir_fifo_buffer;			// Buffer for loading a FIFO, used by FDIR.

/* PUS PACKET FIFOS					*/
/* Initialized in prvInitializeFifos() in main.c	*/
QueueHandle_t hk_to_obc_fifo;			// housekeep	-->		obc_packet_router
QueueHandle_t time_to_obc_fifo;			// time_manage	-->		obc_packet_router
QueueHandle_t mem_to_obc_fifo;			// memory		-->		obc_packet_router
QueueHandle_t sched_to_obc_fifo;		// scheduling	-->		obc_packet_router
QueueHandle_t fdir_to_obc_fifo;			// fdir			-->		obc_packet_router
QueueHandle_t eps_to_obc_fifo;			// eps			-->		obc_packet_router

/* GLOBAL COMMAND FIFOS				*/
/* Initialized in prvInitializeFifos() in main.c	*/
QueueHandle_t obc_to_hk_fifo;			// obc_packet_router	-->		housekeep
QueueHandle_t obc_to_time_fifo;			// obc_packet_router	-->		time_manage
QueueHandle_t obc_to_mem_fifo;			// obc_packet_router	-->		memory
QueueHandle_t obc_to_sched_fifo;		// obc_packet_router	-->		scheduling
QueueHandle_t obc_to_fdir_fifo;			// ob_packet_router		-->		fdir
QueueHandle_t sched_to_hk_fifo;			// scheduling			-->		housekeep
QueueHandle_t sched_to_time_fifo;		// scheduling			-->		time_manage
QueueHandle_t sched_to_memory_fifo;		// scheduling			-->		housekeep

/* ERROR HANDLING FIFOs				*/
QueueHandle_t high_sev_to_fdir_fifo;	// Any task				-->		fdir
QueueHandle_t low_sev_to_fdir_fifo;		// Any task				-->		fdir

QueueHandle_t tc_buffer;
QueueHandle_t tm_buffer;

/* MUTEX LOCKS FOR ERROR HANDLING FIFOs	*/
SemaphoreHandle_t Highsev_Mutex;
SemaphoreHandle_t Lowsev_Mutex;

/* GLOBAL VARIABLES ARE INITIALIZED IN prvInitializeGlobalVars() in main.c */
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
uint8_t opr_data_receivedf;
uint8_t sched_data_receivedf;
uint8_t fdir_data_receivedf;
uint8_t hk_data_receivedf;

/* HOUSEKEEPING COMMAND RESPONSE REGS */
uint32_t hk_read_receive[2];
uint32_t hk_write_receive[2];

/* DATA RECEPTION REGS				  */
uint32_t eps_data_receive[2];
uint32_t coms_data_receive[2];
uint32_t pay_data_receive[2];
uint32_t opr_data_receive[2];
uint32_t sched_data_receive[2];
uint32_t fdir_data_receive[2];
uint32_t hk_data_receive[2];

/*	DATA STORAGE POITNER		   */
uint32_t  glob_stored_data[2];			// Initialized in can_initialize
uint32_t  glob_stored_message[2];		// Initialized in can_initialize

/* Global Mode Variables */
uint32_t	SAFE_MODE;					// Condition which will initially hold the system in safe_mode.
uint32_t	LOW_POWER_MODE;
uint32_t	COMS_TAKEOVER_MODE;
uint32_t	COMS_PAUSED;
uint32_t	EPS_PAUSED;
uint32_t	PAY_PAUSED;
uint32_t	INTERNAL_MEMORY_FALLBACK_MODE;

/* TC/TM Packet flags									*/
uint8_t tm_transfer_completef;
uint8_t start_tm_transferf;
uint8_t current_tc_fullf, receiving_tcf, current_tm_fullf, tm_down_fullf;

/* Global variables for time management	*/
uint8_t ABSOLUTE_DAY;
uint8_t CURRENT_HOUR;
uint8_t CURRENT_MINUTE;
uint8_t CURRENT_SECOND;
uint32_t CURRENT_TIME;
uint8_t absolute_time_arr[4];

/* Global variables for indicating SPI Chip health */
uint8_t SPI_HEALTH1, SPI_HEALTH2, SPI_HEALTH3;

/* Task Handles for each of the running tasks	*/
TaskHandle_t time_manage_HANDLE;
TaskHandle_t memory_manage_HANDLE;
TaskHandle_t opr_HANDLE;
TaskHandle_t housekeeping_HANDLE;
TaskHandle_t eps_HANDLE;
TaskHandle_t coms_HANDLE;
TaskHandle_t pay_HANDLE;
TaskHandle_t scheduling_HANDLE;
TaskHandle_t fdir_HANDLE;
TaskHandle_t wdt_reset_HANDLE;

/* Global variable for determining whether scheduled operations are currently running */
uint8_t scheduling_on;

/* Global variables for determining the timeouts in various operations */
uint32_t req_data_timeout;
uint32_t erase_sector_timeout;
uint32_t chip_erase_timeout;
uint32_t obc_ok_go_timeout;
uint32_t obc_consec_trans_timeout;
uint8_t ssm_consec_trans_timeout;

/* SPI MEMORY BASE ADDRESSES	*/
uint32_t	COMS_BASE;			// COMS = 16kB: 0x00000 - 0x03FFF
uint32_t	EPS_BASE;			// EPS = 16kB: 0x04000 - 0x07FFF
uint32_t	PAY_BASE;			// PAY = 16kB: 0x08000 - 0x0BFFF
uint32_t	HK_BASE;			// HK = 8kB: 0x0C000 - 0x0DFFF
uint32_t	EVENT_BASE;			// EVENT = 8kB: 0x0E000 - 0x0FFFF
uint32_t	SCHEDULE_BASE;		// SCHEDULE = 8kB: 0x10000 - 0x11FFF
uint32_t	CAMERA_BASE;		// CAMERA = 64kB: 0x14000 - 0x23FFF
uint32_t	SCIENCE_BASE;		// SCIENCE = 256kB: 0x24000 - 0x63FFF
uint32_t	TM_BASE;			// TM = 128kB: 0x64000 - 0x83FFF
uint32_t	TC_BASE;			// TC = 128kB: 0x84000 - 0xA3FFF
uint32_t	DIAG_BASE;			// DIAGNOSTICS = 8kB: 0xA4000 - 0xA5FFF
uint32_t	TIME_BASE;			// TIME = 4B: 0xFFFFC - 0xFFFFF

/* Limits for task operations */
uint32_t	MAX_SCHED_COMMANDS;
uint32_t	LENGTH_OF_HK;

/* Payload Global Variables */
uint8_t		pd_collectedf;
uint32_t	science_offset;
uint32_t	downlinked_science_offset;

/* EPS Global Variables */
uint32_t eps_balance_interval, eps_heater_interval;
uint32_t eps_target_temp, eps_temp_interval, active_eps_mode;

/* Global variables for experiment commencement */
uint8_t		experiment_armed;
uint8_t		experiment_started;

/* Variables for keeping track of the PUS Packet Buffer */
uint32_t		NEXT_TM_PACKET;
uint32_t		CURRENT_TM_PACKET;
uint32_t		MAX_TM_PACKETS;
uint32_t		TM_PACKET_COUNT;
uint32_t		TC_PACKET_COUNT;
uint32_t		CURRENT_TC_PACKET;
uint32_t		NEXT_TC_PACKET;



#endif
