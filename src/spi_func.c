/*
	Copyright (c) 2011-2014 Atmel Corporation. All rights reserved.
	Edited by Keenan Burnett

	***********************************************************************
	*	FILE NAME:		spi_func.c
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
	*	DESCRIPTION:
	*
*/

#include "spi_func.h"

/// @cond 0
/**INDENT-OFF**/
#ifdef __cplusplus
extern "C" {
#endif
/**INDENT-ON**/
/// @endcond


/**
 * \brief Set SPI slave transfer.
 *
 * \param p_buf Pointer to buffer to transfer.
 * \param size Size of the buffer.
 */
static void spi_slave_transfer(void *p_buf, uint32_t size)
{
	gs_puc_transfer_buffer = p_buf;
	gs_ul_transfer_length = size;
	gs_ul_transfer_index = 0;
	spi_write(SPI_SLAVE_BASE, gs_puc_transfer_buffer[gs_ul_transfer_index], 0,
			0);
}

/**
 * \brief  SPI command block process.
 */
static void spi_slave_command_process(void)
{
	if (gs_ul_spi_cmd == CMD_END) {
		gs_ul_spi_state = SLAVE_STATE_IDLE;
		gs_spi_status.ul_total_block_number = 0;
		gs_spi_status.ul_total_command_number = 0;
	} else {
		switch (gs_ul_spi_state) {
		case SLAVE_STATE_IDLE:
			/* Only CMD_TEST accepted. */
			if (gs_ul_spi_cmd == CMD_TEST) {
				gs_ul_spi_state = SLAVE_STATE_TEST;
			}
			break;

		case SLAVE_STATE_TEST:
			/* Only CMD_DATA accepted. */
			if ((gs_ul_spi_cmd & CMD_DATA_MSK) == CMD_DATA) {
				gs_ul_spi_state = SLAVE_STATE_DATA;
			}
			gs_ul_test_block_number = gs_ul_spi_cmd & DATA_BLOCK_MSK;
			break;

		case SLAVE_STATE_DATA:
			gs_spi_status.ul_total_block_number++;

			if (gs_spi_status.ul_total_block_number == 
					gs_ul_test_block_number) {
				gs_ul_spi_state = SLAVE_STATE_STATUS_ENTRY;
			}
			break;

		case SLAVE_STATE_STATUS_ENTRY:
			gs_ul_spi_state = SLAVE_STATE_STATUS;
			break;

		case SLAVE_STATE_END:
			break;
		}
	}
}

/**
 * \brief  Start waiting new command.
 */
static void spi_slave_new_command(void)
{
	switch (gs_ul_spi_state) {
	case SLAVE_STATE_IDLE:
	case SLAVE_STATE_END:
		gs_ul_spi_cmd = RC_SYN;
		spi_slave_transfer(&gs_ul_spi_cmd, sizeof(gs_ul_spi_cmd));
		break;

	case SLAVE_STATE_TEST:
		gs_ul_spi_cmd = RC_RDY;
		spi_slave_transfer(&gs_ul_spi_cmd, sizeof(gs_ul_spi_cmd));
		break;

	case SLAVE_STATE_DATA:
		if (gs_spi_status.ul_total_block_number < gs_ul_test_block_number) {
			spi_slave_transfer(gs_uc_spi_buffer, COMM_BUFFER_SIZE);
		}
		break;

	case SLAVE_STATE_STATUS_ENTRY:
		gs_ul_spi_cmd = RC_RDY;
		spi_slave_transfer(&gs_ul_spi_cmd, sizeof(gs_ul_spi_cmd));
		gs_ul_spi_state = SLAVE_STATE_STATUS;
		break;

	case SLAVE_STATE_STATUS:
		gs_ul_spi_cmd = RC_SYN;
		spi_slave_transfer(&gs_spi_status, sizeof(struct status_block_t));
		gs_ul_spi_state = SLAVE_STATE_END;
		break;
	}
}

/**
 * \brief Interrupt handler for the SPI slave.
 */
void SPI_Handler(void)
{
	uint32_t new_cmd = 0;
	static uint16_t data;
	uint8_t uc_pcs;
	uint8_t ret_val = 0;
	uint32_t* reg_ptr = 0x4000800C;		// SPI_TDR (SPI0)
	
	pio_toggle_pin(LED1_GPIO);

	if (spi_read_status(SPI_SLAVE_BASE) & SPI_SR_RDRF) 
	{
		spi_read(SPI_SLAVE_BASE, &data, &uc_pcs);	// SPI message is put into the 16-bit data variable.
		
		//gs_ul_transfer_index++;
		//gs_ul_transfer_length--;
		
		*reg_ptr |= 0x00BB;		// transfer 0xFF back to the SSM.
	}
}

/**
 * \brief Initialize SPI as slave.
 */
static void spi_slave_initialize(void)
{
	uint32_t i;

	/* Reset status */
	gs_spi_status.ul_total_block_number = 0;
	gs_spi_status.ul_total_command_number = 0;
	for (i = 0; i < NB_STATUS_CMD; i++) {
		gs_spi_status.ul_cmd_list[i] = 0;
	}
	gs_ul_spi_state = SLAVE_STATE_DATA;
	gs_ul_spi_cmd = RC_SYN;

	/* Configure an SPI peripheral. */
	spi_enable_clock(SPI_SLAVE_BASE);
	spi_disable(SPI_SLAVE_BASE);
	spi_reset(SPI_SLAVE_BASE);
	spi_set_slave_mode(SPI_SLAVE_BASE);
	spi_disable_mode_fault_detect(SPI_SLAVE_BASE);
	spi_set_peripheral_chip_select_value(SPI_SLAVE_BASE, SPI_CHIP_PCS);
	spi_set_clock_polarity(SPI_SLAVE_BASE, SPI_CHIP_SEL, SPI_CLK_POLARITY);
	spi_set_clock_phase(SPI_SLAVE_BASE, SPI_CHIP_SEL, SPI_CLK_PHASE);
	spi_set_bits_per_transfer(SPI_SLAVE_BASE, SPI_CHIP_SEL, SPI_CSR_BITS_8_BIT);
	spi_enable_interrupt(SPI_SLAVE_BASE, SPI_IER_RDRF);
	spi_enable(SPI_SLAVE_BASE);

	/* Start waiting command. */
	spi_slave_transfer(&gs_ul_spi_cmd, sizeof(gs_ul_spi_cmd));
}

/**
 * \brief Initialize SPI as master.
 */
static void spi_master_initialize(void)
{

	/* Configure an SPI peripheral. */
	spi_enable_clock(SPI_MASTER_BASE);
	spi_disable(SPI_MASTER_BASE);
	spi_reset(SPI_MASTER_BASE);
	spi_set_lastxfer(SPI_MASTER_BASE);
	spi_set_master_mode(SPI_MASTER_BASE);
	spi_disable_mode_fault_detect(SPI_MASTER_BASE);
	spi_set_peripheral_chip_select_value(SPI_MASTER_BASE, SPI_CHIP_PCS);
	spi_set_clock_polarity(SPI_MASTER_BASE, SPI_CHIP_SEL, SPI_CLK_POLARITY);
	spi_set_clock_phase(SPI_MASTER_BASE, SPI_CHIP_SEL, SPI_CLK_PHASE);
	spi_set_bits_per_transfer(SPI_MASTER_BASE, SPI_CHIP_SEL,
			SPI_CSR_BITS_8_BIT);
	spi_set_baudrate_div(SPI_MASTER_BASE, SPI_CHIP_SEL,
			(sysclk_get_cpu_hz() / gs_ul_spi_clock));
	spi_set_transfer_delay(SPI_MASTER_BASE, SPI_CHIP_SEL, SPI_DLYBS,
			SPI_DLYBCT);
	spi_enable(SPI_MASTER_BASE);
}

/**
 * \brief Set the specified SPI clock configuration.
 *
 * \param configuration  Index of the configuration to set.
 */
static void spi_set_clock_configuration(uint8_t configuration)
{
	gs_ul_spi_clock = gs_ul_clock_configurations[configuration];
	spi_master_initialize();
}

/**
 * \brief Perform SPI master transfer.
 *
 * \param pbuf Pointer to buffer to transfer.
 * \param size Size of the buffer.
 */
static void spi_master_transfer(void *p_buf, uint32_t size)
{
	uint32_t i;
	uint8_t uc_pcs;
	static uint16_t data;

	uint8_t *p_buffer;

	p_buffer = p_buf;

	for (i = 0; i < size; i++) {
		spi_write(SPI_MASTER_BASE, p_buffer[i], 0, 0);
		/* Wait transfer done. */
		while ((spi_read_status(SPI_MASTER_BASE) & SPI_SR_RDRF) == 0);
		spi_read(SPI_MASTER_BASE, &data, &uc_pcs);
		p_buffer[i] = data;
	}
}

/**
 * \brief Start SPI transfer test.
 */
static void spi_master_go(void)
{
	uint32_t cmd;
	uint32_t block;
	uint32_t i;

	/* Configure SPI as master, set up SPI clock. */
	spi_master_initialize();

	/*
	 * Send CMD_TEST to indicate the start of test, and device shall return
	 * RC_RDY.
	 */
	while (1) {
		cmd = CMD_TEST;
		spi_master_transfer(&cmd, sizeof(cmd));
		if (cmd == RC_RDY) {
			break;
		}
		if (cmd != RC_SYN) {
			return;
		}
	}
	/* Send CMD_DATA with 4 blocks (64 bytes per page). */
	cmd = CMD_DATA | MAX_DATA_BLOCK_NUMBER;
	spi_master_transfer(&cmd, sizeof(cmd));
	for (block = 0; block < MAX_DATA_BLOCK_NUMBER; block++) {
		for (i = 0; i < COMM_BUFFER_SIZE; i++) {
			gs_uc_spi_buffer[i] = block;
		}
		spi_master_transfer(gs_uc_spi_buffer, COMM_BUFFER_SIZE);
		if (block) {
			for (i = 0; i < COMM_BUFFER_SIZE; i++) {
				if (gs_uc_spi_buffer[i] != (block - 1)) {
					break;
				}
			}
			if (i < COMM_BUFFER_SIZE) {
			} else {
			}
		}
	}

	for (i = 0; i < MAX_RETRY; i++) {
		cmd = CMD_STATUS;
		spi_master_transfer(&cmd, sizeof(cmd));
		if (cmd == RC_RDY) {
			break;
		}
	}
	if (i >= MAX_RETRY) {
		return;
	}

	spi_master_transfer(&gs_spi_status, sizeof(struct status_block_t));

	for (i = 0; i < MAX_RETRY; i++) {
		cmd = CMD_END;
		spi_master_transfer(&cmd, sizeof(cmd));

		if (cmd == RC_SYN) {
			break;
		}
	}

	if (i >= MAX_RETRY) {}
}

/**
 * \brief Application entry point for SPI example.
 *
 * \return Unused (ANSI-C compatibility).
 */
void spi_initialize(void)
{
	uint8_t uc_key;
	uint8_t ret_val = 0;
	uint32_t* reg_ptr = 0x4000800C;		// SPI_TDR (SPI0)
	uint16_t data = 0;

	///* Initialize the SAM system. */
	//sysclk_init();
	//board_init();

	spi_slave_initialize();
	
	*reg_ptr |= 0x00BB;

	/* Configure SPI interrupts for slave only. */
	NVIC_DisableIRQ(SPI_IRQn);
	NVIC_ClearPendingIRQ(SPI_IRQn);
	//NVIC_SetPriority(SPI_IRQn, 0);
	NVIC_EnableIRQ(SPI_IRQn);

	//while (1) {
		//
		//*reg_ptr |= 0x00BB;
		//
		//}	// Put 0xBB in the SPI shift register.
	return;
}

/// @cond 0
/**INDENT-OFF**/
#ifdef __cplusplus
}
#endif
/**INDENT-ON**/
/// @endcond
