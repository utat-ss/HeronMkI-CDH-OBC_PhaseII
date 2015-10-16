/*
	Author: Keenan Burnett, Omar Abdeldayem

	***********************************************************************
	*	FILE NAME:		spi_func.c
	*
	*	PURPOSE:		Serial peripheral interface function for the ATSAM3X8E.
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
	*	08/08/2015			SPI Master mode on the OBC is now functional at a clock rate of 4MHz. The
	*						default chip select pin is pin 10 on the Arduino Due. CS is pulled low during
	*						master transfer, and each master transfer transmits 16 bits.
	*
	*	09/27/2015			Code has been added to spi_master_initialize in order to have different initializations
	*						for chip selects 0,1,2. (3 to be added in the future). **We are now using a variable
	*						peripheral select. NOTE: This means your CS needs to be inserted into your spi_write calls.
	*						I also deleted a bunch of static functions that we weren't really using.
	*
	*						spi_transfer_master has been modified so that it can send an array of bytes while keeping
	*						CS low. This has no impact on code up to this point which used 16-bit SPI and a length of 1.
	*						
	*	DESCRIPTION:
	*
	*			At the moment, the most important part of this file is the SPI interrupt handler.
	*
	*			The OBC is able to function as both an SPI Master and Slave. Currently, the defualt initialization
	*			sets up the OBC as Master.
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
 * \brief Interrupt handler for the SPI slave.
 */
void SPI_Handler(void)
{
	static uint16_t data;
	uint8_t uc_pcs;
	uint32_t* reg_ptr = 0x4000800C;		// SPI_TDR (SPI0)

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
	uint32_t spi_chip_sel, spi_clk_freq, spi_clk_pol, spi_clk_pha;
	spi_enable_clock(SPI_MASTER_BASE);
	spi_reset(SPI_MASTER_BASE);
	spi_set_master_mode(SPI_MASTER_BASE);
	spi_disable_mode_fault_detect(SPI_MASTER_BASE);
	spi_disable_loopback(SPI_MASTER_BASE);

	spi_set_peripheral_chip_select_value(SPI_MASTER_BASE, spi_get_pcs(2));	// This sets the value of PCS within the Mode Register.
	spi_set_variable_peripheral_select(SPI_MASTER_BASE);					// PCS needs to be set within each transfer (PCS within SPI_TDR).
	spi_disable_peripheral_select_decode(SPI_MASTER_BASE);					// Each CS is to be connected to a single device.
	spi_set_delay_between_chip_select(SPI_MASTER_BASE, SPI_DLYBCS);

	/* Set communication parameters for CS0	*/
	spi_chip_sel = 0;
	spi_clk_freq = 100000;	// SPI CLK for RTC = 100kHz.
	spi_clk_pol = 1;
	spi_clk_pha = 0;
	spi_set_transfer_delay(SPI_MASTER_BASE, spi_chip_sel, SPI_DLYBS,
			SPI_DLYBCT);
	spi_set_bits_per_transfer(SPI_MASTER_BASE, spi_chip_sel, SPI_CSR_BITS_16_BIT);
	spi_set_baudrate_div(SPI_MASTER_BASE, spi_chip_sel, spi_calc_baudrate_div(spi_clk_freq, sysclk_get_cpu_hz())); 
	spi_configure_cs_behavior(SPI_MASTER_BASE, spi_chip_sel, SPI_CS_RISE_FORCED);		// CS rises after SPI transfers have completed.
	spi_set_clock_polarity(SPI_MASTER_BASE, spi_chip_sel, spi_clk_pol);
	spi_set_clock_phase(SPI_MASTER_BASE, spi_chip_sel, spi_clk_pha);
	
	/* Set communication parameters for CS1	*/
	spi_chip_sel = 1;
	spi_clk_freq = 2000000;	// SPI CLK for RTC = 4MHz.
	spi_clk_pol = 0;
	spi_clk_pha = 0;
	spi_set_transfer_delay(SPI_MASTER_BASE, spi_chip_sel, SPI_DLYBS,
	SPI_DLYBCT);
	spi_set_bits_per_transfer(SPI_MASTER_BASE, spi_chip_sel, SPI_CSR_BITS_8_BIT);
	spi_set_baudrate_div(SPI_MASTER_BASE, spi_chip_sel, spi_calc_baudrate_div(spi_clk_freq, sysclk_get_cpu_hz())); 
	spi_configure_cs_behavior(SPI_MASTER_BASE, spi_chip_sel, SPI_CS_RISE_FORCED);
	spi_set_clock_polarity(SPI_MASTER_BASE, spi_chip_sel, spi_clk_pol);
	spi_set_clock_phase(SPI_MASTER_BASE, spi_chip_sel, spi_clk_pha);
	
	/* Set communication parameters for CS2	*/
	spi_chip_sel = 2;
	spi_clk_freq = 44000000;	// SPI CLK for MEM2 = 44MHz.
	spi_clk_pol = 1;
	spi_clk_pha = 0;
	spi_set_transfer_delay(SPI_MASTER_BASE, spi_chip_sel, SPI_DLYBS,
	SPI_DLYBCT);
	spi_set_bits_per_transfer(SPI_MASTER_BASE, spi_chip_sel, SPI_CSR_BITS_8_BIT);
	spi_set_baudrate_div(SPI_MASTER_BASE, spi_chip_sel, spi_calc_baudrate_div(spi_clk_freq, sysclk_get_cpu_hz()));
	spi_configure_cs_behavior(SPI_MASTER_BASE, spi_chip_sel, SPI_CS_KEEP_LOW);
	spi_set_clock_polarity(SPI_MASTER_BASE, spi_chip_sel, spi_clk_pol);
	spi_set_clock_phase(SPI_MASTER_BASE, spi_chip_sel, spi_clk_pha);
	
	/* Enable SPI Communication */
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
void spi_master_transfer(void *p_buf, uint32_t size, uint8_t chip_sel)
{
	uint32_t i = 0;
	uint8_t pcs;
	pcs = spi_get_pcs(chip_sel);
	uint16_t data;
	uint8_t	timeout = 84;	// ~1us timeout for getting the read status back.

	uint16_t *p_buffer;

	p_buffer = p_buf;
	
	if(size == 1)	// Only transfer a single message.
	{
		spi_write(SPI_MASTER_BASE, p_buffer[i], pcs, 1);
		// The last parameter above tells SPI whether this is the last byte to be transferred.
		/* Wait transfer done. */
		while ((spi_read_status(SPI_MASTER_BASE) & SPI_SR_RDRF) == 0);
		spi_read(SPI_MASTER_BASE, &data, &pcs);
		p_buffer[i] = data;
		return;
	}
	
	// Keep CS low for the duration of the transfer, set high @ end.
	for (i = 0; i < (size - 1); i++) 
	{
		spi_write(SPI_MASTER_BASE, p_buffer[i], pcs, 0);	
		/* Wait transfer done. */
		while ((spi_read_status(SPI_MASTER_BASE) & SPI_SR_RDRF) == 0);
		spi_read(SPI_MASTER_BASE, &data, &pcs);
		p_buffer[i] = data;
		delay_us(100);
	}
	delay_us(100);
	spi_write(SPI_MASTER_BASE, p_buffer[(size - 1)], pcs, 1);
	/* Wait transfer done. */
	while ((spi_read_status(SPI_MASTER_BASE) & SPI_SR_RDRF) == 0);
	spi_read(SPI_MASTER_BASE, &data, &pcs);
	p_buffer[(size - 1)] = data;
	return;
}

void spi_master_transfer_keepcslow(void *p_buf, uint32_t size, uint8_t chip_sel)
{
	uint32_t i = 0;
	uint8_t pcs;
	pcs = spi_get_pcs(chip_sel);
	uint16_t data;

	uint16_t *p_buffer;

	p_buffer = p_buf;
		
	// Keep CS low for the duration of the transfer, keep low @ end.
	for (i = 0; i < size; i++) 
	{
		spi_write(SPI_MASTER_BASE, p_buffer[i], pcs, 0);	
		/* Wait transfer done. */
		while ((spi_read_status(SPI_MASTER_BASE) & SPI_SR_RDRF) == 0);
		spi_read(SPI_MASTER_BASE, &data, &pcs);
		p_buffer[i] = data;
	}
	return;
}

void spi_master_read(void *p_buf, uint32_t size, uint32_t chip_sel)
{
	uint32_t i;
	uint8_t pcs;
	pcs = spi_get_pcs(chip_sel);
	uint16_t data;
	uint16_t *p_buffer;
	p_buffer = p_buf;

	for (i = 0; i < size; i++)
	{
		spi_write(SPI_MASTER_BASE, 0, pcs, 0);
		/* Wait transfer done. */
		while ((spi_read_status(SPI_MASTER_BASE) & SPI_SR_RDRF) == 0);
		spi_read(SPI_MASTER_BASE, &data, &pcs);
		p_buffer[i] = (uint8_t)data;
	}
}

/**
 * \brief Initialize the ATSAM3X8E SPI driver in Master mode.
 *
 * \return Unused (ANSI-C compatibility).
 */
void spi_initialize(void)
{
	//uint32_t* reg_ptr = 0x4000800C;		// SPI_TDR (SPI0)
		
	//*reg_ptr |= 0x00BB;
	//spi_slave_initialize();
	spi_master_initialize();

	return;
}

/// @cond 0
/**INDENT-OFF**/
#ifdef __cplusplus
}
#endif
/**INDENT-ON**/
/// @endcond
