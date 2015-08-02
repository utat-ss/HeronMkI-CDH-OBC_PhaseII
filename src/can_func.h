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
	*	07/07/2015		I changed 'decode_can_msg' to 'debug_can_msg' and added the function 'store_can_msg'.
	*
*/

#include <asf/sam/components/can/sn65hvd234.h>
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

SemaphoreHandle_t	Can1_Mutex;

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

#define HK_TRANSMIT					0x12345678
#define CAN_MSG_DUMMY_DATA          0xFFFFFFFF

#define DUMMY_COMMAND				0XFFFFFFFF
#define MSG_ACK						0xABABABAB

#define HK_RETURNED					0XF0F0F0F0
#define HK_REQUEST					0x0F0F0F0F

#define DATA_REQUEST				0x55555555
#define DATA_RETURNED				0x00000055

#define MESSAGE_RETURNED			0X00000000

/* IDs for receiving OBC mailboxes */
#define CAN1_MB0				0x00
#define CAN1_MB1				0x01
#define CAN1_MB2				0x02
#define CAN1_MB3				0x03
#define CAN1_MB4				0x04
#define CAN1_MB5				0x05
#define CAN1_MB6				0x06
#define CAN1_MB7				0x07

/* IDs for transmitting OBC mailboxes */
#define CAN0_MB0				0x08
#define CAN0_MB1				0x09
#define CAN0_MB2				0x0A
#define CAN0_MB3				0x0B
#define CAN0_MB4				0x0C
#define CAN0_MB5				0x0D
#define CAN0_MB6				0x0E
#define CAN0_MB7				0x0F

#define SUB0_ID0				20
#define SUB0_ID1				21
#define SUB0_ID2				22
#define SUB0_ID3				23
#define SUB0_ID4				24
#define SUB0_ID5				25

///* IDs for COMS/SUB0 mailboxes */
//#define SUB0_ID0				0x10
//#define SUB0_ID1				0x11
//#define SUB0_ID2				0x12
//#define SUB0_ID3				0x13
//#define SUB0_ID4				0x14
//#define SUB0_ID5				0x15

/* IDs for EPS/SUB1 mailboxes */
#define SUB1_MB0				0x16
#define SUB1_MB1				0x17
#define SUB1_MB2				0x18
#define SUB1_MB3				0x19
#define SUB1_MB4				0x1A
#define SUB1_MB5				0x1B

/* IDs for PAYLOAD/SUB2 mailboxes */
#define SUB2_MB0				0x1C
#define SUB2_MB1				0x1D
#define SUB2_MB2				0x1E
#define SUB2_MB3				0x1F
#define SUB2_MB4				0x20
#define SUB2_MB5				0x21

/* MessageType_ID  */
#define MT_DATA					0x00
#define MT_HK					0x01
#define MT_COM					0x02
#define MT_TC					0x03

/* SENDER_ID */
#define COMS_ID					0x00
#define EPS_ID					0x01
#define PAYL_ID					0x02
#define OBC_ID					0xFF

/* COMMAND SMALL-TYPE: */
#define REQ_RESPONSE			0x01
#define REQ_DATA				0x02

#define SMALLTYPE_DEFAULT		0x00

#define COMMAND_PRIO			10
#define HK_REQUEST_PRIO			20
#define DATA_PRIO				25

/* CAN frame max data length */
#define MAX_CAN_FRAME_DATA_LEN      8

/* CAN0 Transceiver */
sn65hvd234_ctrl_t can0_transceiver;

/* CAN1 Transceiver */
sn65hvd234_ctrl_t can1_transceiver;

/* CAN0 Transfer mailbox structure */
can_mb_conf_t canT_MB;

/* CAN1 Transfer mailbox structure */
can_mb_conf_t canR_MB;

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
uint32_t send_can_command(uint32_t low, uint32_t high, uint32_t ID, uint32_t PRIORITY);				// API Function.
uint32_t request_housekeeping(uint32_t ID);															// API Function.
uint32_t read_can_data(uint32_t* message_high, uint32_t* message_low, uint32_t access_code);		// API Function.
uint32_t read_can_tc(uint32_t* message_high, uint32_t* message_low, uint32_t access_code);			// API Function.
uint32_t read_can_hk(uint32_t* message_high, uint32_t* message_low, uint32_t access_code);			// API Function.
uint32_t read_can_coms(uint32_t* message_high, uint32_t* message_low, uint32_t access_code);		// API Function.
uint32_t high_command_generator(uint8_t SENDER_ID, uint8_t MessageType, uint8_t smalltype);			// API Function.

