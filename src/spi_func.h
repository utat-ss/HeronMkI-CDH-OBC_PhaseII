/*
	Author: Keenan Burnett, Omar Abdeldayem

	***********************************************************************
	*	FILE NAME:		spi_func.h
	*
	*	PURPOSE:		Serial peripheral interface function for the ATSAM3X8E.
	*	
	*
	*	FILE REFERENCES:		asf.h, stdio_serial.h, conf_board.h, conf_clock.h, conf_spi.h, pio.h
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
	*
	*	DEVELOPMENT HISTORY:
	*	04/30/2015			Created.
	*
	*   08/08/2015			Modified SPI clock phase for compatibility with the DS3234 RTC.
	*
	*	09/27/2015			SPI Clock phase, pol, freq, etc is specific to each Chip Select, see
	*						spi_master_initialize to change these values in the future.
	*						NOTE: RTC and MEM1 are currently connected to CS1, and MEM2 is
	*						connected to CS2. (In the future, we'll fix this so that RTC is on CS0).
	*	
	* 	DESCRIPTION:
	*
*/

#ifndef SPI_FUNC_H
#define SPI_FUNC_H

#include "asf.h"
//#include "stdio_serial.h"
#include "conf_board.h"
#include "conf_clock.h"
#include <asf/sam/drivers/spi/conf_spi.h>
//#include "conf_spi.h"
#include "pio.h"
#include "gpio.h"

/* SPI clock frequency (Hz) */
#define SPI_CLK_FREQ 4000000

/* Chip select. */
#define SPI_CHIP_SEL 2	// At the moment CS2 is connected to MEM2.
#define SPI_CHIP_PCS spi_get_pcs(SPI_CHIP_SEL)

/* Clock polarity. */
#define SPI_CLK_POLARITY 1

/* Clock phase. */
#define SPI_CLK_PHASE 1

/* Delay before SPCK. */
#define SPI_DLYBS 0x00	// was 0x45

/* Delay between consecutive transfers. */
#define SPI_DLYBCT 0x02	// was 0x02

/* Delay between consecutive chip selects */
#define SPI_DLYBCS 0x00


/* SPI slave states for this example. */
#define SLAVE_STATE_IDLE           0
#define SLAVE_STATE_TEST           1
#define SLAVE_STATE_DATA           2
#define SLAVE_STATE_STATUS_ENTRY   3
#define SLAVE_STATE_STATUS         4
#define SLAVE_STATE_END            5

/* SPI example commands for this example. */
/* slave test state, begin to return RC_RDY. */
#define CMD_TEST     0x10101010

/* Slave data state, begin to return last data block. */
#define CMD_DATA     0x29380000

/* Slave status state, begin to return RC_RDY + RC_STATUS. */
#define CMD_STATUS   0x68390384

/* Slave idle state, begin to return RC_SYN. */
#define CMD_END      0x68390484

/* General return value. */
#define RC_SYN       0x55AA55AA

/* Ready status. */
#define RC_RDY       0x12345678

/* Slave data mask. */
#define CMD_DATA_MSK 0xFFFFFFFF		// was 0xFFFF0000

/* Slave data block mask. */
#define DATA_BLOCK_MSK 0x0000FFFF

/* Number of commands logged in status. */
#define NB_STATUS_CMD   20

/* Number of SPI clock configurations. */
#define NUM_SPCK_CONFIGURATIONS 4

/* SPI Communicate buffer size. */
#define COMM_BUFFER_SIZE   64

/* UART baudrate. */
#define UART_BAUDRATE      115200

/* Data block number. */
#define MAX_DATA_BLOCK_NUMBER  4

/* Max retry times. */
#define MAX_RETRY    4

/* Status block. */
struct status_block_t {
	/** Number of data blocks. */
	uint32_t ul_total_block_number;
	/** Number of SPI commands (including data blocks). */
	uint32_t ul_total_command_number;
	/** Command list. */
	uint32_t ul_cmd_list[NB_STATUS_CMD];
};

/* SPI clock setting (Hz). */
static uint32_t gs_ul_spi_clock = 62500;

/* Current SPI return code. */
static uint32_t gs_ul_spi_cmd = RC_SYN;

/* Current SPI state. */
static uint32_t gs_ul_spi_state = 0;

/* 64 bytes data buffer for SPI transfer and receive. */
static uint8_t gs_uc_spi_buffer[COMM_BUFFER_SIZE];

/* Pointer to transfer buffer. */
static uint8_t *gs_puc_transfer_buffer;

/* Transfer buffer index. */
static uint32_t gs_ul_transfer_index;

/* Transfer buffer length. */
static uint32_t gs_ul_transfer_length;

/* SPI Status. */
static struct status_block_t gs_spi_status;

static uint32_t gs_ul_test_block_number;

/* SPI clock configuration. */
static const uint32_t gs_ul_clock_configurations[] =
{ 500000, 1000000, 2000000, 5000000 };

void SPI_Handler(void);
void spi_master_transfer(void *p_buf, uint32_t size, uint8_t chip_sel);
void spi_master_transfer_keepcslow(void *p_buf, uint32_t size, uint8_t chip_sel);
void spi_master_read(void *p_buf, uint32_t size, uint32_t chip_sel);

#endif
