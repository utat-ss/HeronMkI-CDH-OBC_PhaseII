/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved
Modified By: Keenan Burnett
***********************************************************************
*	FILE NAME:		conf_board.h
*
*	PURPOSE:
*	Board configuration File.
*
*	FILE REFERENCES:	None.
*
*	EXTERNAL VARIABLES:		None.
*
*	EXTERNAL REFERENCES:	Many, see dependencies diagram on dropbox.
*
*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
*
*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None.
*
*	NOTES:
*	The generic board support module includes board-specific definitions
*	and function prototypes, such as the board initialization function.
*
*	Uncommenting different 'defines' will enabled certain parts of the board
*	to be configured upon the next compilation.
*
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*	None.
*
*	DESCRIPTION:	Not Required.
*
*	DEVELOPMENT HISTORY:
*	11/29/2014			Header Changed.
*
*						Line 85: Removed the commenting such that CONF_BOARD_KEEP_WATCHDOG_AT_INIT
*						was defined. (This allows the watchdog to be initialized in init.c)
*
*	12/20/2014			I added lines 52-54 in order to allow CAN0 and CAN1 pins to be initialized.
*
*	02/28/2015			I added lines 106-114 in order to implement the usart example program.
*
*	04/11/2016			Deleted a whole bunch of lines which were previously commented out.
*						Added line 119 for the temperature sensor which was added to the IT5 board.
*/

#ifndef CONF_BOARD_H_INCLUDED
#define CONF_BOARD_H_INCLUDED

/* Enabled Watchdog */
//#define CONF_BOARD_KEEP_WATCHDOG_AT_INIT

/* Enabled CAN Controllers*/
#define CONF_BOARD_CAN0

#define CONF_BOARD_CAN1

/*
* USART pins are configured as basic serial port by default.
* You can enable other pins according application.
*/
#define CONF_BOARD_UART_CONSOLE

/* USART0 module is used in serial mode. */
#define CONF_BOARD_USART_RXD
#define CONF_BOARD_USART_TXD

#define BOARD_ID_USART             ID_USART0

#define BOARD_USART                USART0

#define BOARD_USART_BAUDRATE       38400

#define USART_Handler              USART0_Handler

#define USART_IRQn                 USART0_IRQn

/* Configure USART CTS pin */
//#define CONF_BOARD_USART_CTS

/* Configure USART RTS pin */
//#define CONF_BOARD_USART_RTS

/* Configure USART synchronous communication SCK pin */
//#define CONF_BOARD_USART_SCK

/** SPI0 MACRO definition */
#define CONF_BOARD_SPI0

/** SPI0 slave select0 MACRO definition */
#define CONF_BOARD_SPI0_NPCS0
#define CONF_BOARD_SPI0_NPCS1
#define CONF_BOARD_SPI0_NPCS2
#define CONF_BOARD_SPI0_NPCS3

/** Spi Hw ID . */
#define SPI_ID          ID_SPI0

/** SPI base address for SPI master mode*/
#define SPI_MASTER_BASE      SPI0
/** SPI base address for SPI slave mode, (on different board) */
#define SPI_SLAVE_BASE       SPI0

/*	SPI0 Slave Select CS1 Configuration		*/
#define CONF_BOARD_SPI0_NPCS1_GPIO SPI0_NPCS1_PB20_GPIO
#define CONF_BOARD_SPI0_NPCS1_FLAGS SPI0_NPCS1_PB20_FLAGS

/*	SPI0 Slave Select CS2 Configuration		*/
#define CONF_BOARD_SPI0_NPCS2_GPIO SPI0_NPCS2_PB21_GPIO
#define CONF_BOARD_SPI0_NPCS2_FLAGS SPI0_NPCS2_PB21_FLAGS

/*	SPI0 Slave Select CS3 Configuration		*/
#define CONF_BOARD_SPI0_NPCS3_GPIO SPI0_NPCS3_PB23_GPIO
#define CONF_BOARD_SPI0_NPCS3_FLAGS SPI0_NPCS3_PB23_FLAGS

/*  SPI Memory 								*/
#define CONF_SPI_MEM1

/*	Temperature Sensor						*/
#define TEMP_SENSOR

#endif /* CONF_BOARD_H_INCLUDED */
