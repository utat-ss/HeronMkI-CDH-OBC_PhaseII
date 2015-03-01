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

/** Receive buffer. */
static uint8_t gs_puc_buffer[2][USART_BUFFER_SIZE];

/** Next Receive buffer. */
static uint8_t gs_puc_nextbuffer[2][USART_BUFFER_SIZE];

/** Current bytes in buffer. */
static uint32_t gs_ul_size_buffer = USART_BUFFER_SIZE;

/** Current bytes in next buffer. */
static uint32_t gs_ul_size_nextbuffer = USART_BUFFER_SIZE;

/** Byte mode read buffer. */
static uint32_t gs_ul_read_buffer = 0;

/** Current transfer mode. */
static uint8_t gs_uc_trans_mode = PDC_TRANSFER;

/** Buffer number in use. */
static uint8_t gs_uc_buf_num = 0;

/** PDC data packet. */
pdc_packet_t g_st_packet, g_st_nextpacket;

/** Pointer to PDC register base. */
Pdc *g_p_pdc;

/** Flag of one transfer end. */
static uint8_t g_uc_transend_flag = 0;

/**
 * \brief Interrupt handler for USART. Echo the bytes received and start the
 * next receive.
 */
void USART_Handler(void)
{
	uint32_t ul_status;

	/* Read USART Status. */
	ul_status = usart_get_status(BOARD_USART);

	if (gs_uc_trans_mode == PDC_TRANSFER) {
		/* Receive buffer is full. */
		if (ul_status & US_CSR_RXBUFF) {
			/* Disable timer. */
			tc_stop(TC0, 0);

			/* Echo back buffer. */
			g_st_packet.ul_addr =									
					(uint32_t)gs_puc_buffer[gs_uc_buf_num];			// This is the first tx packet.
					
			if ( *(gs_puc_buffer[gs_uc_buf_num]) == 0x61)
			{
				pio_toggle_pin(LED2_GPIO);
			}
			
			g_st_packet.ul_size = gs_ul_size_buffer;
			g_st_nextpacket.ul_addr =
					(uint32_t)gs_puc_nextbuffer[gs_uc_buf_num];		// This is the second tx packet.
			g_st_nextpacket.ul_size = gs_ul_size_nextbuffer;
			pdc_tx_init(g_p_pdc, &g_st_packet, &g_st_nextpacket);	// This causes the message to be sent.

			if (g_uc_transend_flag) {
				gs_ul_size_buffer = USART_BUFFER_SIZE;
				gs_ul_size_nextbuffer = USART_BUFFER_SIZE;
				g_uc_transend_flag = 0;
			}

			gs_uc_buf_num = MAX_BUF_NUM - gs_uc_buf_num;

			/* Restart read on buffer. */
			g_st_packet.ul_addr =
					(uint32_t)gs_puc_buffer[gs_uc_buf_num];
			g_st_packet.ul_size = USART_BUFFER_SIZE;
			g_st_nextpacket.ul_addr =
					(uint32_t)gs_puc_nextbuffer[ gs_uc_buf_num];
			g_st_nextpacket.ul_size = USART_BUFFER_SIZE;
			pdc_rx_init(g_p_pdc, &g_st_packet, &g_st_nextpacket);	// This causes us to start reading again.

			/* Restart timer. */
			tc_start(TC0, 0);
		}
	} else {
		/* Transfer without PDC. */
		if (ul_status & US_CSR_RXRDY) {
			usart_getchar(BOARD_USART, (uint32_t *)&gs_ul_read_buffer);
			usart_write(BOARD_USART, gs_ul_read_buffer);
		}
	}
}

/**
 * \brief Interrupt handler for TC0. Record the number of bytes received,
 * and then restart a read transfer on the USART if the transfer was stopped.
 */
void TC0_Handler(void)
{
	uint32_t ul_status;
	uint32_t ul_byte_total = 0;

	/* Read TC0 Status. */
	ul_status = tc_get_status(TC0, 0);

	/* RC compare. */
	if (((ul_status & TC_SR_CPCS) == TC_SR_CPCS) &&
			(gs_uc_trans_mode == PDC_TRANSFER)) {
		/* Flush PDC buffer. */
		ul_byte_total = USART_BUFFER_SIZE - pdc_read_rx_counter(g_p_pdc);
		if ((ul_byte_total != 0) && (ul_byte_total != USART_BUFFER_SIZE)) {
			/* Log current size. */
			g_uc_transend_flag = 1;
			if (pdc_read_rx_next_counter(g_p_pdc) == 0) {
				gs_ul_size_buffer = USART_BUFFER_SIZE;
				gs_ul_size_nextbuffer = ul_byte_total;
			} else {
				gs_ul_size_buffer = ul_byte_total;
				gs_ul_size_nextbuffer = 0;
			}

			/* Trigger USART Receive Buffer Full Interrupt. */
			pdc_rx_clear_cnt(g_p_pdc);
		}
	}
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
 * \brief Configure Timer Counter 0 (TC0) to generate an interrupt every 200ms.
 * This interrupt will be used to flush USART input and echo back.
 */
void configure_tc(void)
{
	uint32_t ul_div;
	uint32_t ul_tcclks;
	static uint32_t ul_sysclk;

	/* Get system clock. */
	ul_sysclk = sysclk_get_cpu_hz();

	/* Configure PMC. */
	pmc_enable_periph_clk(ID_TC0);

	/* Configure TC for a 50Hz frequency and trigger on RC compare. */
	tc_find_mck_divisor(TC_FREQ, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC0, 0, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC0, 0, (ul_sysclk / ul_div) / TC_FREQ);

	/* Configure and enable interrupt on RC compare. */
	NVIC_EnableIRQ((IRQn_Type)ID_TC0);
	tc_enable_interrupt(TC0, 0, TC_IER_CPCS);
}

/**
 * \brief Reset the TX & RX, and clear the PDC counter.
 */
void usart_clear(void)
{
	/* Reset and disable receiver & transmitter. */
	usart_reset_rx(BOARD_USART);
	usart_reset_tx(BOARD_USART);

	/* Clear PDC counter. */
	g_st_packet.ul_addr = 0;
	g_st_packet.ul_size = 0;
	g_st_nextpacket.ul_addr = 0;
	g_st_nextpacket.ul_size = 0;
	pdc_rx_init(g_p_pdc, &g_st_packet, &g_st_nextpacket);

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

	/* Initialize the SAM system. */
	sysclk_init();
	board_init();

	/* Output example information. */

	/* Configure USART. */
	configure_usart();

	/* Get board USART PDC base address. */
	g_p_pdc = usart_get_pdc_base(BOARD_USART);
	/* Enable receiver and transmitter. */
	pdc_enable_transfer(g_p_pdc, PERIPH_PTCR_RXTEN | PERIPH_PTCR_TXTEN);

	/* Configure TC. */
	configure_tc();

	/* Start receiving data and start timer. */
	g_st_packet.ul_addr = (uint32_t)gs_puc_buffer[gs_uc_buf_num];
	g_st_packet.ul_size = USART_BUFFER_SIZE;
	g_st_nextpacket.ul_addr = (uint32_t)gs_puc_nextbuffer[gs_uc_buf_num];
	g_st_nextpacket.ul_size = USART_BUFFER_SIZE;
	pdc_rx_init(g_p_pdc, &g_st_packet, &g_st_nextpacket);

	gs_uc_trans_mode = PDC_TRANSFER;

	usart_disable_interrupt(BOARD_USART, US_IDR_RXRDY);
	usart_enable_interrupt(BOARD_USART, US_IER_RXBUFF);
	
	tc_start(TC0, 0);
}


