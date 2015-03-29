/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		usart_func.c
	*
	*	PURPOSE:		This file contains functions and handlers related to usart communication.
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
	*	03/27/2015			Changed the transfer mode to BYTE TRANSFER
	*						
	*						Added helper functions check_command, check_string, and usart_initialize.
	*
	*						Deleted all code relating to PDC transfer mode.
	*
	*						Essentially we no longer have a software implemented buffer, however it's
	*						not really that big of a deal considering this code is only for a demonstration.
	*
	*	DESCRIPTION:
	*
	*	Open a terminal which connects to the appropriate serial port
	*	with Tera Term to connect with the on-board USART port.
	*	Then the program works in ECHO mode, so USART will
	*	send back anything it receives from the HyperTerminal.  You can send a text
	*	file from the HyperTerminal connected with USART port to the device (without
	*	any protocol such as X-modem).
	*
	*	Build the program and download it into the evaluation boards.
	*	Connect a mini USB cable to your FTDI programmer and connect
	*	the ground, 5V (Or 3.3v), ground, TXO (FTDI) to RX1 (DUE), and
	*	RXI (FTDI) to TX1 (DUE).
	*
	*	On your computer, open and configure a terminal application
	*    ( TeraTerm) with these settings:
	*   - 115200 bauds
	*   - 8 bits of data
	*   - No parity
	*   - 1 stop bit
	*   - No flow control
	*	- Select the COM port (Can be found under control panel > devices and printers)
	*
	*	You will now be able to enter commands in the following form:
	*
*/

#include "usart_func.h"

/** Byte mode read buffer. */
static uint32_t gs_ul_read_buffer = 0;

/** Current transfer mode. */
static uint8_t gs_uc_trans_mode = BYTE_TRANSFER;

static uint8_t command_start = 0;		// THis flag indicates that the following characters are for a command.

static uint8_t command_end = 0;			// This flag indicates that the command has been finished.

static uint8_t command_array[10];		// This array will hold the characters which were went during the "listening" period.

static uint8_t array_pos = 0;			// This integer is needed to remember the position in the array we are writing to in between interrupts.

/**
 * \brief Interrupt handler for USART. Echo the bytes received and start the
 * next receive.
 */
void USART_Handler(void)
{
	uint32_t ul_status;
	uint32_t new_char = 0, new_char2 = 0;	// For ease of reading, I have created this variable.
	uint8_t command_completed = 0;
	uint8_t i = 0;

	/* Read USART Status. */
	ul_status = usart_get_status(BOARD_USART);
	
	pio_toggle_pin(LED4_GPIO);

	if (gs_uc_trans_mode == BYTE_TRANSFER)
	{
		/* Transfer without PDC. */
		if (ul_status & US_CSR_RXRDY) 
		{
			usart_getchar(BOARD_USART, (uint32_t *)&gs_ul_read_buffer);
			new_char = gs_ul_read_buffer;
			
			if (new_char == 0x31)									// The character '1' was received, start "listening".
				command_start = 1;
				
			if (new_char == 0x32)									// The character '2' was received, execute command.
				command_end = 1;
				
			if ((command_start == 1) && (new_char != 0) && (new_char != 0x31) && new_char != 0x32)			// Since we are listening, we store the new characters.
			{
				command_array[array_pos % 10] = new_char;
				array_pos ++;
			}
			// '1' and '2' were both received, execute command.
			if ((command_end == 1) && (command_start == 1))
			{
				// Check command function.
				check_command();
				command_end = 0;
				command_start = 0;
				array_pos = 0;
					
				for (i = 0; i < 10; i ++)
				{
					command_array[i] = 0;
				}
				command_completed = 1;
			}

			if (!command_completed)				
				usart_write(BOARD_USART, gs_ul_read_buffer);
					
			command_completed = 0;
				
		}
	}
}

/************************************************************************/
/*		CHECK WHETHER TO EXECUTE TO THE USART COMMAND                   */
/*																		*/
/*		This function will check the contents of the USART command		*/
/*		which was sent via a computer terminal and determine what		*/
/*		action to take.													*/
/************************************************************************/

void check_command(void)
{	
	uint32_t character = 0;
	
	char* message_array;
	
	char* check_array;
	
	uint8_t i = 0;
	uint8_t hk = 1;
	uint8_t sad = 1;
	
	// Housekeeping requested. "hk" was sent.
	check_array = "hk";
	
	hk =  check_string(check_array);
	
	check_array = "i am sad";
	
	sad = check_string(check_array);
	
	if (hk == 1)
	{	
		
		message_array = "\n\rSYSTEMS ARE NOMINAL, SIR.\n\r";
				
		while(*message_array)
		{
			character = *message_array;
			while(usart_write(BOARD_USART, character));	// Send the character.
			
			message_array++;
		}
		
		if(data_reg[0] == 0x550003ff)
		{
			message_array = "\n\rSUBSYSTEM TEMPERATURE IS 22 C\n\r";			
		}

		while(*message_array)
		{
			character = *message_array;
			while(usart_write(BOARD_USART, character));	// Send the character.
			
			message_array++;
		}
	}
	
	if (sad == 1)
	{	
		
		message_array = "\n\rDO YOU WANT A BISCUIT?\n\r";
		
		while(*message_array)
		{
			character = *message_array;
			while(usart_write(BOARD_USART, character));	// Send the character.
			
			message_array++;
		}
	}
	
	return;
}

/************************************************************************/
/*		CHECK STRING                                                    */
/*																		*/
/*		This function checks whether the string which is passed to it	*/
/*		is equal to the array of chars in command_array (which is		*/
/*		defined globally.												*/	
/************************************************************************/

uint8_t check_string(char* str_to_check)
{
	uint8_t	i = 0;
	uint8_t ret_val = 1;
	
	char* temp_str;
	
	temp_str = str_to_check;
	
	for (i = 0; i < 10; i++)
	{
		if (*temp_str != command_array[i])
		{
			ret_val = 0;
			break;
		}
		if (!*temp_str)
			break;
			
		temp_str++;
	}
	
	return ret_val;
}

/**
 * \brief Configure USART in normal (serial rs232) mode, asynchronous,
 * 8 bits, 1 stop bit, no parity, 115200 bauds and enable its transmitter
 * and receiver.
 */
void configure_usart(void)
{
	const sam_usart_opt_t usart_console_settings = {
		BOARD_USART_BAUDRATE,
		US_MR_CHRL_8_BIT,
		US_MR_PAR_NO,
		US_MR_NBSTOP_1_BIT,
		US_MR_CHMODE_NORMAL,
		/* This field is only used in IrDA mode. */
		0
	};

	/* Enable the peripheral clock in the PMC. */
	sysclk_enable_peripheral_clock(BOARD_ID_USART);

	/* Configure USART in serial mode. */
	usart_init_rs232(BOARD_USART, &usart_console_settings,
			sysclk_get_cpu_hz());

	/* Disable all the interrupts. */
	usart_disable_interrupt(BOARD_USART, ALL_INTERRUPT_MASK);

	/* Enable the receiver and transmitter. */
	usart_enable_tx(BOARD_USART);
	usart_enable_rx(BOARD_USART);

	/* Configure and enable interrupt of USART. */
	NVIC_EnableIRQ(USART_IRQn);
}

/**
 * \brief Reset the TX & RX, and clear the PDC counter.
 */
void usart_clear(void)
{
	/* Reset and disable receiver & transmitter. */
	usart_reset_rx(BOARD_USART);
	usart_reset_tx(BOARD_USART);

	/* Enable receiver & transmitter. */
	usart_enable_tx(BOARD_USART);
	usart_enable_rx(BOARD_USART);
}

/**
 * \brief Application entry point for usart_serial example.
 *
 * \return Unused (ANSI-C compatibility).
 */
void usart_initialize(void)
{
	uint8_t uc_char;
	uint8_t uc_flag;
	uint8_t i = 0;
	char* message_array;
	uint32_t character = 0;

	/* Configure USART. */
	configure_usart();

	gs_uc_trans_mode = BYTE_TRANSFER;
	
	for (i = 0; i < 10; i++)
	{
		command_array[i] = 0;
	}

	usart_enable_interrupt(BOARD_USART, US_IDR_RXRDY);
	usart_disable_interrupt(BOARD_USART, US_IER_RXBUFF);
	
	message_array = "WHAT CAN I DO FOR YOU, SIR?\n\r";
		
	while(*message_array)
	{
		character = *message_array;
		while(usart_write(BOARD_USART, character));	// Send the character.
			
		message_array++;
	}
	
	return;
}

