/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		can_func.h
	*
	*	PURPOSE:		CAN related includes and structures, and includes for can_func.c
	*	
	*
	*	FILE REFERENCES:	sn65hvd234.h, can.h, stdio.h, string.h, board.h, sysclk.h, exceptions.h
	*						pmc.h, conf_board.h, conf_clock.h, pio.h, FreeRTOS.h, task.h, semphr.h, 
	*						global_var.h
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
	*
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
	*	New tasks should be written to use as much of CMSIS as possible. The ASF and
	*	FreeRTOS API libraries should also be used whenever possible to make the program
	*	more portable.

	*	DEVELOPMENT HISTORY:
	*	01/02/2015		Created.
	*
	*	02/01/2015		Added a prototype for send_can_command().
	*
	*	02/18/2015		Added a prorotype for request_housekeeping().
	*
	*					Added to the list of ID and message definitions in order to communicate
	*					more effectively with the STK600.
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
	*	07/07/2015		I changed 'decode_can_msg' to 'debug_can_msg' and added the function 'stora_can_msg'
	*
*/

#include <asf/sam/drivers/can/can.h>

#include <stdio.h>
#include <string.h>
#include "board.h"
#include "sysclk.h"
#include "exceptions.h"
#include "pmc.h"
#include "conf_board.h"
#include "conf_clock.h"
#include "pio.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "global_var.h"		// Contains convenient global variables.


typedef struct {
	uint32_t ul_mb_idx;
	uint8_t uc_obj_type;  /**< Mailbox object type, one of the six different
	                         objects. */
	uint8_t uc_id_ver;    /**< 0 stands for standard frame, 1 stands for
	                         extended frame. */
	uint8_t uc_length;    /**< Received data length or transmitted data
	                         length. */
	uint8_t uc_tx_prio;   /**< Mailbox priority, no effect in receive mode. */
	uint32_t ul_status;   /**< Mailbox status register value. */
	uint32_t ul_id_msk;   /**< No effect in transmit mode. */
	uint32_t ul_id;       /**< Received frame ID or the frame ID to be
	                         transmitted. */
	uint32_t ul_fid;      /**< Family ID. */
	uint32_t ul_datal;
	uint32_t ul_datah;
} can_temp_t;

/*		CAN MUTEXES DEFINED HERE		*/
SemaphoreHandle_t	Can0_Mutex;

/*		CURRENT PRIORITY LEVELS			
	Note: ID and priority are two different things.
		  For the sake of simplicity, I am making them the same.

		COMS TO CDH COMMAND (IMMED)	0
		PAYLOAD COMMAND				1
		COMS TO CDH COMMAND (SCHED) 2
		EPS COMMAND					3
		COMS COMMND					4
		COMS REQUESTING DATA		5
		COMS RECEIVING DATA			6
		RECEIVING PAYLOAD DATA		7
		REQUEST PAYLOAD DATA		8
		REQUEST HOUSEKEEPING		20
		TRANMITTING HOUSEKEEPING	15
		LED TOGGLE (LOWEST + 1) =	11
*/

#define COMMAND_OUT					0X01010101
#define COMMAND_IN					0x11111111

#define HK_TRANSMIT					0x12345678
#define CAN_MSG_DUMMY_DATA          0xFFFFFFFF

#define DUMMY_COMMAND				0XFFFFFFFF
#define MSG_ACK						0xABABABAB

#define HK_RETURNED					0XF0F0F0F0
#define HK_REQUEST					0x0F0F0F0F

#define DATA_REQUEST				0x55555555
#define DATA_RETURNED				0x00000055

#define MESSAGE_RETURNED			0X00000000

#define CAN0_MB0				1
#define CAN0_MB1				2
#define CAN0_MB2				3
#define CAN0_MB3				4
#define CAN0_MB4				5
#define CAN0_MB5				6
#define CAN0_MB6				7
#define CAN0_MB7				8

#define CAN1_MB0				10
#define CAN1_MB1				10
#define CAN1_MB2				10
#define CAN1_MB3				10
#define CAN1_MB4				14
#define CAN1_MB5				14
#define CAN1_MB6				14
#define CAN1_MB7				17

/* IDs for COMS/SUB0 mailboxes */
#define SUB0_ID0				20
#define SUB0_ID1				21
#define SUB0_ID2				22
#define SUB0_ID3				23
#define SUB0_ID4				24
#define SUB0_ID5				25

/* IDs for EPS/SUB1 mailboxes */
#define SUB1_ID0				26
#define SUB1_ID1				27
#define SUB1_ID2				28
#define SUB1_ID3				29
#define SUB1_ID4				30
#define SUB1_ID5				31

/* IDs for PAYLOAD/SUB2 mailboxes */
#define SUB2_ID0				32
#define SUB2_ID1				33
#define SUB2_ID2				34
#define SUB2_ID3				35
#define SUB2_ID4				36
#define SUB2_ID5				37

/* MessageType_ID  */
#define MT_DATA					0x00
#define MT_HK					0x01
#define MT_COM					0x02
#define MT_TC					0x03

/* SENDER_ID */
#define COMS_ID					0x00
#define EPS_ID					0x01
#define PAY_ID					0x02
#define OBC_ID					0x03
#define HK_TASK_ID				0x04
#define DATA_TASK_ID			0x05
#define TIME_TASK_ID			0x06
#define COMS_TASK_ID			0x07
#define EPS_TASK_ID				0x08
#define PAY_TASK_ID				0x09
#define OBC_PACKET_ROUTER_ID	0x0A
#define SCHEDULING_TASK_ID		0x0B
#define FDIR_TASK_ID			0x0C
#define WD_RESET_TASK_ID		0x0D
#define MEMORY_TASK_ID			0x0E
#define HK_GROUND_ID			0x10
#define TIME_GROUND_ID			0x11
#define MEM_GROUND_ID			0x12
#define GROUND_PACKET_ROUTER_ID 0x13
#define FDIR_GROUND_ID			0x14
#define SCHED_GROUND_ID			0x15

/* COMMAND SMALL-TYPE: */
#define REQ_RESPONSE			0x01
#define REQ_DATA				0x02
#define REQ_HK					0x03
#define RESPONSE 				0x04
#define REQ_READ				0x05
#define ACK_READ				0x06
#define REQ_WRITE				0x07
#define ACK_WRITE				0x08
#define SET_SENSOR_HIGH			0x09
#define SET_SENSOR_LOW			0x0A
#define SET_VAR					0x0B
#define SET_TIME				0x0C
#define SEND_TM					0x0D
#define SEND_TC					0x0E
#define TM_PACKET_READY			0x0F
#define OK_START_TM_PACKET		0x10
#define TC_PACKET_READY			0x11
#define OK_START_TC_PACKET		0x12
#define TM_TRANSACTION_RESP		0x13
#define TC_TRANSACTION_RESP		0x14
#define SAFE_MODE_TYPE			0x15
#define SEND_EVENT				0x16
#define ASK_OBC_ALIVE			0x17
#define OBC_IS_ALIVE			0x18

/* Checksum only */
#define SAFE_MODE_VAR			0x09

#define SMALLTYPE_DEFAULT		0x00

/* DATA SMALL-TYPE	   */
#define SPI_TEMP1				0xFF
#define COMS_PACKET				0xFE

/* MESSAGE PRIORITIES	*/
#define COMMAND_PRIO			25
#define HK_REQUEST_PRIO			20
#define DATA_PRIO				10
#define DEF_PRIO				10

/* SENSOR NAMES			*/
#define PANELX_V				0x01
#define PANELX_I				0x02
#define PANELY_V				0x03
#define PANELY_I				0x04
#define BATTM_V					0x05
#define BATT_V					0x06
#define BATTIN_I				0x07
#define BATTOUT_I				0x08
#define BATT_TEMP				0x09
#define EPS_TEMP				0x0A
#define COMS_V					0x0B
#define COMS_I					0x0C
#define PAY_V					0x0D
#define PAY_I					0x0E
#define OBC_V					0x0F
#define OBC_I					0x10
#define BATT_I					0x11
#define COMS_TEMP				0x12
#define OBC_TEMP				0x13
#define PAY_TEMP0				0x14
#define PAY_TEMP1				0x15
#define PAY_TEMP2				0x16
#define PAY_TEMP3				0x17
#define PAY_TEMP4				0x18
#define PAY_HUM					0x19
#define PAY_PRESS				0x1A
#define PAY_ACCEL				0x1B

/* VARIABLE NAMES		*/
#define MPPTA					0xFF
#define MPPTB					0xFE
#define COMS_MODE				0xFD
#define EPS_MODE				0xFC
#define PAY_MODE				0xFB
#define OBC_MODE				0xFA
#define PAY_STATE				0xF9
#define ABS_TIME_D				0xF8
#define ABS_TIME_H				0xF7
#define ABS_TIME_M				0xF6
#define ABS_TIME_S				0xF5
#define SPI_CHIP_1				0xF4
#define SPI_CHIP_2				0xF3
#define SPI_CHIP_3				0xF2	

/* CAN frame max data length */
#define MAX_CAN_FRAME_DATA_LEN      8

/* CAN0 Transfer mailbox structure */
can_mb_conf_t can0_mailbox;

/* CAN1 Transfer mailbox structure */
can_mb_conf_t can1_mailbox;

can_temp_t temp_mailbox_C0;
can_temp_t temp_mailbox_C1;

/* Function Prototypes */

void CAN1_Handler(void);
void CAN0_Handler(void);
void debug_can_msg(can_mb_conf_t *p_mailbox, Can* controller);
void reset_mailbox_conf(can_mb_conf_t *p_mailbox);
void can_initialize(void);
uint32_t can_init_mailboxes(uint32_t x);
void save_can_object(can_mb_conf_t *original, can_temp_t *temp);
void restore_can_object(can_mb_conf_t *original, can_temp_t *temp);
void store_can_msg(can_mb_conf_t *p_mailbox, uint8_t mb);
int send_can_command(uint32_t low, uint8_t byte_four, uint8_t sender_id, uint8_t ssm_id, uint8_t smalltype, uint8_t priority);	// API Function.
uint32_t send_can_command_h(uint32_t low, uint32_t high, uint32_t ID, uint32_t PRIORITY);			// API and Helper.
uint32_t request_housekeeping(uint32_t ssm_id);															// API Function.
uint32_t read_can_data(uint32_t* message_high, uint32_t* message_low, uint32_t access_code);		// API Function.
uint32_t read_can_msg(uint32_t* message_high, uint32_t* message_low, uint32_t access_code);			// API Function.
uint32_t read_can_hk(uint32_t* message_high, uint32_t* message_low, uint32_t access_code);			// API Function.
uint32_t read_can_coms(uint32_t* message_high, uint32_t* message_low, uint32_t access_code);		// API Function.
uint32_t high_command_generator(uint8_t sender_id, uint8_t ssm_id, uint8_t MessageType, uint8_t smalltype);	// API Function.
void decode_can_command(can_mb_conf_t *p_mailbox, Can* controller);
void alert_can_data(can_mb_conf_t *p_mailbox, Can* controller);
uint8_t read_from_SSM(uint8_t sender_id, uint8_t ssm_id, uint8_t passkey, uint8_t addr);			// API Function.
uint8_t write_to_SSM(uint8_t sender_id, uint8_t ssm_id, uint8_t passkey, uint8_t addr, uint8_t data);	// API Function.
uint32_t request_sensor_data(uint8_t sender_id, uint8_t ssm_id, uint8_t sensor_name, int* status);	// API Function.
int set_sensor_high(uint8_t sender_id, uint8_t ssm_id, uint8_t sensor_name, uint16_t boundary);		// API Function.
int set_sensor_low(uint8_t sender_id, uint8_t ssm_id, uint8_t sensor_name, uint16_t boundary);		// API Function.
int set_variable(uint8_t sender_id, uint8_t ssm_id, uint8_t var_name, uint16_t value);				// API Function.

