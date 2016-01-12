/*
	Author: Keenan Burnett

	***********************************************************************
*	FILE NAME:		ssm_programming.c
*
*	PURPOSE:		This file is to be used for reprogramming SSM via an ICSP connection
*					between the On-Board Computer and Subsystem Computers. Each of which
*					consists of a SPI connection to the SSM's alternate SPI lines and a reset pin.
*
*	FILE REFERENCES:		ssm_programming.h
*
*	EXTERNAL VARIABLES:
*
*	EXTERNAL REFERENCES:	Same a File References.
*
*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
*
*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
*
*	NOTES:		The program used to reprogram each SSM needs to be stored in SPI memory in order for a programming
*				action to be able to occur.
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
*	DEVELOPMENT HISTORY:
*	01/12/2016			Created.
*
*	DESCRIPTION:
*
*/

#include "ssm_programming.h"


int reprogram_ssm(uint8_t ssmID)
{
	uint32_t length = 0;
	uint32_t base = 0;
	uint8_t tries = 10;
	int ret_val = -1;

	if(ssmID > 2)
		return -1;		// Invalid ssmID
	
	if(!ssmID)
		base = COMS_BASE;
	if(ssmID == 1)
		base = PAY_BASE;
	if(ssmID == 2)
		base = EPS_BASE;
	
	length = spimem_read(base, &length, 4);
	if(!length)
		return -2;		// Program needs to have a nonzero length
	
	if (xSemaphoreTake(Spi0_Mutex, (TickType_t) 1000) == pdTRUE)	// Block for up to one second on the Spi0_Mutex.
	{
		enter_atomic();		// Uninterrupted sequence of actions below
		
		gpio_set_pin_high(SPI0_MEM1_HOLD);	// Turn memory holding on for SPIMEM1

		ret_val = initialize_reprogramming(ssmID);
		if(ret_val < 0)
		{
			xSemaphoreGive(Spi0_Mutex);
			exit_atomic();
			gpio_set_pin_low(SPI0_MEM1_HOLD);	// Turn memory holding off for SPIMEM1
			ret_val -= 3;
			return ret_val;		// -4 : Synchronization failed, -5 : Incorrect Device Signature, -6 : Chip Erase Failed
		}
		
		ret_val = upload_mem_to_ssm(length, base);
		while(tries-- & (ret_val < 0))
		{ 
			ret_val = upload_mem_to_ssm(length, base);
		}
		if(ret_val < 0)
		{
			xSemaphoreGive(Spi0_Mutex);
			exit_atomic();
			gpio_set_pin_low(SPI0_MEM1_HOLD);	// Turn memory holding off for SPIMEM1
			ret_val -= 6;
			return ret_val;		// -7: Writing to SSM failed, 
		}
		
		xSemaphoreGive(Spi0_Mutex);
		exit_atomic();
		gpio_set_pin_low(SPI0_MEM1_HOLD);	// Turn memory holding off for SPIMEM1
		return 1;
	}
	else
		return -3;		// SPI is currently being used.
		
}

// Puts the SSM in reset mode
// Sends the command "Program Enable"
// Verifies the device signature
// Erases the SSM's memory so that it is ready to be programmed.
int initialize_reprogramming(uint8_t ssmID)
{	
	uint32_t enable = 0;
	uint8_t tries = 10;
	uint32_t RST = 0;
	/* Reset the SSM */
	if(!ssmID)
		RST = COMS_RST_GPIO;
	if(ssmID == 1)
		RST = EPS_RST_GPIO;
	if(ssmID == 2)
		RST = PAY_RST_GPIO;
	gpio_set_pin_low(RST);
			
	delay_ms(20);
	
	enable = 0xAC530000;
	spi_master_transfer(&enable, 4, 1);		// Send the Program Enable command
	enable &= 0x0000FF00;
	enable = enable >> 8;
	
	while(tries-- && (enable != 0x53))		// Keep trying until command succeeds
	{
		gpio_set_pin_high(RST);				// Pulse the reset pin to try again
		delay_us(1);
		gpio_set_pin_low(RST);
		enable = 0xAC530000;
		spi_master_transfer(&enable, 4, 1);
		enable &= 0x0000FF00;
		enable = enable >> 8;
	}
	if(enable != 0x53)
		return -1;							// Synchronization failed
	
	/* Check that the device signature is correct */
	if(read_signature() != 0x84951E)
		return -2;							// Incorrect Device Signature
	
	enable = CHIP_ERASE;
	spi_master_transfer(&enable, 4, 1);
	delay_ms(10);
	
	enable = READ_PROG_MEM | 0x00100000;
	spi_master_transfer(&enable, 4, 1);
	enable &= 0x000000FF;
	if(enable != 0xFF)
		return -3;							// Chip Erase Failed
	
	return 1;
}

// To be used only when the Spi0_Mutex has been acquired.
uint32_t read_signature(void)
{
	// Byte 0.
	uint32_t signature = 0, temp = READ_SIGNATURE;
	spi_master_transfer(&temp, 4, 1);
	signature = temp & 0x000000FF;
	// Byte 1
	temp = READ_SIGNATURE | 0x00000100;
	spi_master_transfer(&temp, 4, 1);
	signature += (temp & 0x000000FF) << 8;
	// Byte 1
	temp = READ_SIGNATURE | 0x00000200;
	spi_master_transfer(&temp, 4, 1);
	signature += (temp & 0x000000FF) << 12;	
	return signature;
}

int upload_mem_to_ssm(uint32_t size, uint32_t base)
{
	uint32_t i, j, pages = 0, leftover = 0;
	uint8_t byte = 0;
	uint32_t oddness = 0, temp = 0;
	pages = (size / 128);
	leftover = size % 128;
	
	// Figure out the number of pages to be written to.
	
	// Don't forget to wait for the page write to finish.
	
	// Remember to check that the write succeeded afterwards.
	
	for(j = 0; j < pages; j++)
	{
		spi_master_transfer(write_buff, 128, 1);
		for(i = 0; i < 128; i++)
		{
			if(i % 2)
				oddness = 0x08000000;
			else
				oddness = 0;
			temp = LOAD_PAGE_BYTE | oddness | ((i/2) << 8) | (uint32_t)write_buff[i];
			spi_master_transfer(&temp, 4, 1);
		}
		temp = WRITE_PAGE | (j << 14);
		spi_master_transfer(&temp, 4, 1);
		// Check that the write was correct here.
	}
	
	return;
}

