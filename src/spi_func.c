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
	*	03/27/2016			Fixing the function headers for this file.
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

/************************************************************************/
/* SPI_HANDLER				                                            */
/* @Purpose: This interrupt handler is to be used when the OBC is set up*/
/* to be a slave on the SPI bus.										*/
/************************************************************************/
void SPI_Handler(void)
{
	static uint16_t data;
	uint8_t uc_pcs;
	uint32_t* reg_ptr = 0x4000800C;		// SPI_TDR (SPI0)

	if (spi_read_status(SPI_SLAVE_BASE) & SPI_SR_RDRF) 
	{
		spi_read(SPI_SLAVE_BASE, &data, &uc_pcs);	// SPI message is put into the 16-bit data variable.		
		*reg_ptr |= 0x00BB;		// transfer 0xFF back to the SSM.
	}
}

/************************************************************************/
/* SPI_SLAVE_INITIALIZE				                                    */
/* @Purpose: Initializes the OBC as a SPI slave.						*/
/************************************************************************/
static void spi_slave_initialize(void)
{
	//uint32_t i;
	/* Reset status */
	//gs_spi_status.ul_total_block_number = 0;
	//gs_spi_status.ul_total_command_number = 0;
	//for (i = 0; i < NB_STATUS_CMD; i++) {
		//gs_spi_status.ul_cmd_list[i] = 0;
	//}
	//
	//gs_ul_spi_state = SLAVE_STATE_DATA;
	//gs_ul_spi_cmd = RC_SYN;

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
	//spi_slave_transfer(&gs_ul_spi_cmd, sizeof(gs_ul_spi_cmd));
}

/************************************************************************/
/* SPI_MASTER_INITIALIZE				                                */
/* @Purpose: Initializes the OBC as a SPI master.						*/
/************************************************************************/
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
	spi_clk_freq = 100000;
	spi_clk_pol = 1;
	spi_clk_pha = 0;
	spi_set_transfer_delay(SPI_MASTER_BASE, spi_chip_sel, SPI_DLYBS,
			SPI_DLYBCT);
	spi_set_bits_per_transfer(SPI_MASTER_BASE, spi_chip_sel, SPI_CSR_BITS_16_BIT);
	spi_set_baudrate_div(SPI_MASTER_BASE, spi_chip_sel, spi_calc_baudrate_div(spi_clk_freq, sysclk_get_cpu_hz())); 
	spi_configure_cs_behavior(SPI_MASTER_BASE, spi_chip_sel, SPI_CS_RISE_FORCED);		// CS rises after SPI transfers have completed.
	spi_set_clock_polarity(SPI_MASTER_BASE, spi_chip_sel, spi_clk_pol);
	spi_set_clock_phase(SPI_MASTER_BASE, spi_chip_sel, spi_clk_pha);
	
	/* Set communication parameters for CS1	*/ // Currently being used for RTC
	spi_chip_sel = 1;
	spi_clk_freq = 2000000;	// SPI CLK for RTC = 4MHz.
	spi_clk_pol = 1;
	spi_clk_pha = 0;
	spi_set_transfer_delay(SPI_MASTER_BASE, spi_chip_sel, 0x45,
	0x02);
	spi_set_bits_per_transfer(SPI_MASTER_BASE, spi_chip_sel, SPI_CSR_BITS_16_BIT);
	spi_set_baudrate_div(SPI_MASTER_BASE, spi_chip_sel, spi_calc_baudrate_div(spi_clk_freq, sysclk_get_cpu_hz()));
	spi_configure_cs_behavior(SPI_MASTER_BASE, spi_chip_sel, SPI_CS_KEEP_LOW);
	spi_set_clock_polarity(SPI_MASTER_BASE, spi_chip_sel, spi_clk_pol);
	spi_set_clock_phase(SPI_MASTER_BASE, spi_chip_sel, spi_clk_pha);
	
	/* Set communication parameters for CS2	*/ // Currently being used for SPIMEM
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

/************************************************************************/
/* SPI_MASTER_TRANSFER				                                    */
/* @Purpose: Sends the contents of *p_buf as well as reads the incoming	*/
/* bytes into *p_buf.													*/
/* @param: *p_buf: Pointer to the data to be written to SPI memory, data*/
/* read from the SPI bus will also be placed into this buffer.			*/
/*	(Just make this uint16_t* to be safe)								*/
/* @NOTE: Each byte within *p_buf should be spaced with a zero to the	*/
/* left of it. EX: uint16_t* buf. buf[0] = 0x00AB.						*/
/* The reason this is done is so that you can easily switch to 16bit	*/
/* transfers in which case all 16 bits are used.						*/
/* @param: size: Number of bytes in p_buf								*/
/* @param: chip_sel: (0|1|2|3) determines which hardware chipselect is	*/
/* being used. Each CS has it's own SPI settings.						*/
/* @NOTE: CS will be kept low for the duration of these transfers.		*/
/************************************************************************/
void spi_master_transfer(void *p_buf, uint32_t size, uint8_t chip_sel)
{
	uint32_t i = 0;
	uint8_t pcs;
	pcs = spi_get_pcs(chip_sel);
	uint16_t data;
	//uint8_t	timeout = 84;	// ~1us timeout for getting the read status back.

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

/************************************************************************/
/* SPI_MASTER_TRANSFER_KEEP_CS_LOW		                                */
/* @Purpose: Same as above, except that at the end CS is not driven high*/
/************************************************************************/
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

/************************************************************************/
/* SPI_MASTER_READ						                                */
/* @Purpose: Sometimes it is desirable to simply read the incoming bytes*/
/* on the SPI bus when 0 is transferred to the slave.					*/
/************************************************************************/
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

uint16_t spi_retrieve_temp(void)
{
	uint16_t* msg_ptr;
	uint16_t ret_val;
	*msg_ptr = 0;
	gpio_set_pin_high(SPI0_MEM1_HOLD);		// Hold SPIMEM1
	gpio_set_pin_low(TEMP_SS);
	spi_master_transfer_keepcslow(msg_ptr, 1, 1);
	delay_us(128);
	*msg_ptr = 0;
	spi_master_transfer_keepcslow(msg_ptr, 1, 1);
	ret_val = (*msg_ptr) << 8;
	delay_us(128);
	*msg_ptr = 0;
	spi_master_transfer_keepcslow(msg_ptr, 1, 1);
	gpio_set_pin_high(TEMP_SS);
	gpio_set_pin_low(SPI0_MEM1_HOLD);		// Hold-off SPIMEM1
	ret_val += msg_ptr;
	return ret_val;
}

/************************************************************************/
/* SPI_INITIALIZE						                                */
/* @Purpose: Initializes SPI registers for the OBC.						*/
/************************************************************************/
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
