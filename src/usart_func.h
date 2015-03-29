/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		usart_func.h
	*
	*	PURPOSE:		This file contains includes and definitions for usart_func.c
	*	
	*
	*	FILE REFERENCES:		string.h, asf.h, stdio_serial.h, conf_board.h, conf_clock.h
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
	*	02/28/2015			Created.
	*
*/


#include <string.h>
#include "asf.h"
#include <asf/common/utils/stdio/stdio_serial/stdio_serial.h>
#include "conf_board.h"
#include "conf_clock.h"
#include "global_var.h"

/** Size of the receive buffer used by the PDC, in bytes. */
#define USART_BUFFER_SIZE         100

/** USART PDC transfer type definition. */
#define PDC_TRANSFER        1

/** USART FIFO transfer type definition. */
#define BYTE_TRANSFER       0

/** Max buffer number. */
#define MAX_BUF_NUM         1

/** All interrupt mask. */
#define ALL_INTERRUPT_MASK  0xffffffff

/** Timer counter frequency in Hz. */
#define TC_FREQ             1

/*
 * Functions Prototypes.
 */
void USART_Handler(void);
void TC0_Handler(void);
void configure_usart(void);
void configure_tc(void);
void usart_clear(void);
void usart_initialize(void);
void check_command(void);
uint8_t check_string(char* str_to_check);

/*-----------------------------------------------------------*/

