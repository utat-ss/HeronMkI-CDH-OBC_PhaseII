/*
Author: Keenan Burnett
***********************************************************************
* FILE NAME: error_handling.h
*
* PURPOSE:
* This file is to be used for the includes, prototypes, and definitions related to error_handling.c
*
* EXTERNAL VARIABLES:
*
* FILE REFERENCES: global_var.h, stdint.h, FreeRTOS.h, semphr.h, task.h, can_func.h
*
* ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
*
* ASSUMPTIONS, CONSTRAINTS, CONDITIONS: None
*
* NOTES:
*
* REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
* DEVELOPMENT HISTORY:
* 11/26/2015		Created.
* 12/05/2015		Added Error IDs.
*
*/

#include "global_var.h"
#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"
#include "task.h"
#include "can_func.h"


/* Global signals tasks can wait on during High-Sev errors */
uint8_t hk_fdir_signal;
uint8_t time_fdir_signal;
uint8_t coms_fdir_signal;
uint8_t eps_fdir_signal;
uint8_t pay_fdir_signal;
uint8_t opr_fdir_signal;
uint8_t sched_fdir_signal;
uint8_t wdt_fdir_signal;
uint8_t mem_fdir_signal;
uint8_t high_error_array[152];
uint8_t low_error_array[152];
// SSMs will have their own local variables for doing this
// can_func.c will not be allowed to block on any signal, only error reports.

// Note that currently acquired mutex locks should be released during the waiting process.

//ERROR IDs as defined in FDIR document
	//Note: check these with the cases in decode_error in fdir.c. I think some might be off.

#define SCHED_SPIMEM_R_ERROR			0x01 //implemented
#define SCHED_SPIMEM_W_ERROR			0x02 //implemented
#define SCHED_SPIMEM_CHIP_ERROR			0x03 //Does 0x01 and 0x02 cover this case? 
#define SCHED_COMMAND_EXEC_ERROR		0x04 //Almost implemented, check check_schedule() for details
#define SCHED_FIFO_RW_ERROR		        0x05 //done
#define HK_FIFO_RW_ERROR				0x06 //implemented
#define HK_COLLECT_ERROR				0x07 //implemented
#define HK_SPIMEM_R_ERROR				0x08 //implemented
#define HK_SPIMEM_W_ERROR				0x1C //implemented
#define TM_FIFO_RW_ERROR				0x09 //done
#define SPIMEM_BUSY_CHIP_ERROR			0x0A //done    //all errors in spimem.c send msg_buff as data
#define SPIMEM_CHIP_ERASE_ERROR			0x0B //done
#define SPIMEM_LOAD_SECTOR_ERROR		0x0C //done
#define SPIMEM_UPDATE_SPIBUFFER_ERROR	0x0D //done
#define SPIMEM_ERASE_SECTOR_ERROR		0x0E //done
#define SPIMEM_WRITE_SECTOR_ERROR		0x0F //done
#define SPIMEM_WR_ERROR					0x10 //done
#define SPIMEM_ALL_CHIPS_ERROR			0x11 //done; assuming this is a highsev error?
#define RTC_SPIMEM_R_ERROR				0x12 //done; not sure about what to send as data
#define MEM_SPIMEM_CHIPS_ERROR			0x13 //done: hi or low sev?
#define MEM_SPIMEM_MEM_WASH_ERROR		0x14 //done  //added errorREPORT for spimem_read as well in mem_manage.c
#define MEM_OTHER_SPIMEM_ERROR			0x15 //done
#define MEM_FIFO_RW_ERROR				0x16  //todo: same solution as SCHED_FIFO_RW_ERROR
#define EPS_SSM_GET_SENSOR_DATA_ERROR	0x17 //done
#define EPS_SET_VARIABLE_ERROR			0x18 //done
#define OBC_COMS_TC_TM_ERROR			0x19 // ignore
#define OBC_TC_PACKET_ERROR				0x1A //There is no FDIR sequence for this!! -> goes to SAFE_MODE
#define OBC_FIFO_RW_ERROR				0x1B  //todo: same solution as SCHED_FIFO_RW_ERROR
#define TC_OK_GO_TIMED_OUT				0x1C
#define TC_CONSEC_TIMED_OUT				0x1D
#define TM_OK_GO_TIMED_OUT				0x1E
#define TM_CONSEC_TIMED_OUT				0x1F
#define PAY_SPIMEM_RW_ERROR				0x20

int errorREPORT(uint8_t task, uint8_t code, uint32_t error, uint32_t* data);
int errorASSERT(uint8_t task, uint8_t code, uint32_t error, uint8_t* data, SemaphoreHandle_t mutex);
void xQueueSendToBackTask(uint8_t task, uint8_t direction, QueueHandle_t fifo, uint8_t *itemToQueue, TickType_t ticks);
void xQueueReceiveTask(uint8_t task, uint8_t direction, QueueHandle_t fifo, uint8_t *itemToQueue, TickType_t ticks);
