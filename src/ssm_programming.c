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


/************************************************************************/
/* REPROGRAM_SSM	                                                    */
/* 																		*/
/* @param: ssmID: ID of the SSM to be reprogrammed (0|1|2)				*/
/* @purpose: This function will take the SSM program which should be	*/
/* stored in SPI memory at its respective base address and then			*/
/* proceeds to put the SSM in reset mode, erases it's flash and then	*/
/* reprograms it using an ICSP (SPI) interface with that SSM.			*/
/* @return: -1 = Invalid SSM, -2 = Program has zero length, 			*/
/* -3 = SPI is currently being used, -4 = Synchronization failed.		*/
/* -5 = Incorrect device signature, -6 = Chip Erase Failed.				*/
/* -7 = Writing to SSM failed. 1 = Success								*/
/************************************************************************/
int reprogram_ssm(uint8_t ssmID)
{
	uint32_t length = 0;
	uint8_t msg[4];
	uint32_t base = 0, RST = 0;
	uint8_t tries = 10;
	int ret_val = -1;

	if(ssmID > 2)
		return -1;		// Invalid ssmID
	
	if(!ssmID)
	{
		base = COMS_BASE;
		RST = COMS_RST_GPIO;
	}
	if(ssmID == 1)
	{
		base = PAY_BASE;
		RST = COMS_RST_GPIO;
	}
	if(ssmID == 2)
	{
		base = EPS_BASE;
		RST = COMS_RST_GPIO;
	}
	
	spimem_read(base, msg, 4);
	length = (uint32_t)msg[0];
	length += ((uint32_t)msg[1]) << 8;
	length += ((uint32_t)msg[2]) << 16;
	length += ((uint32_t)msg[3]) << 24;
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
			gpio_set_pin_high(RST);
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
			gpio_set_pin_high(RST);
			ret_val -= 6;
			return ret_val;		// -7: Writing to SSM failed, 
		}
		
		xSemaphoreGive(Spi0_Mutex);
		exit_atomic();
		gpio_set_pin_low(SPI0_MEM1_HOLD);	// Turn memory holding off for SPIMEM1
		gpio_set_pin_high(RST);
		return 1;
	}
	else
		return -3;		// SPI is currently being used.		
}

/************************************************************************/
/* INITIALIZE_REPOGRAMMING                                              */
/* 																		*/
/* @param: ssmID: ID of the SSM to be reprogrammed (0|1|2)				*/
/* @purpose: This function puts the SSM in reset mode, sends the command*/
/* "Program Enable", verifies the device signature and then erase the	*/
/* SSM'd memory so that it is ready to be programmed.					*/
/* @return: -1 = Synchronization failed, 1 = Success					*/
/* -2 = Incorrect device signature, -3 = Chip Erase Failed.				*/
/************************************************************************/
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

/************************************************************************/
/* READ_SIGNATURE		                                                */
/* 																		*/
/* @param: None.														*/
/* @purpose: After the SSM is put in reset mode and synchronization		*/
/* was successful, this function can be used to return device signature	*/
/* @return: device signature											*/
/* @Note: To be used only when the Spi0_Mutex has been acquired.		*/
/************************************************************************/
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

/************************************************************************/
/* READ_SIGNATURE		                                                */
/* 																		*/
/* @param: size: Length of the program memory to be loaded into the SSM */
/* @param: base: The base address in SPIMEM where program is located.	*/
/* @purpose: Loads the program in SPI memory into the SSM's flash memory*/
/* @return: -1 = Write Failed, 1 = Success								*/
/* @Note: To be used only when the Spi0_Mutex has been acquired.		*/
/************************************************************************/
int upload_mem_to_ssm(uint32_t size, uint32_t base)
{
	uint32_t i, j, pages = 0, leftover = 0, position = 0;
	uint32_t oddness = 0, temp = 0;
	pages = (size / 128);
	leftover = size % 128;
	
	for(j = 0; j < pages; j++)
	{
		clear_write_buff();
		spi_master_transfer(write_buff, 128, 1);
		/* Write the bytes into the SSM's program memory */
		for(i = 0; i < 128; i++)
		{
			if(i % 2)
				oddness = 0x08000000;
			else
				oddness = 0;
			if(write_buff[i] != 0xFF)
				position = i;
			temp = (LOAD_PAGE_BYTE) | oddness | ((i/2) << 8) | (uint32_t)write_buff[i];
			spi_master_transfer(&temp, 4, 1);
		}
		/* Write the page */
		temp = (WRITE_PAGE) | (j << 14);
		spi_master_transfer(&temp, 4, 1);
		delay_ms(5);
		if(position % 2)
			oddness = 0x08000000;
		else
			oddness = 0;
		temp = (READ_PROG_MEM) | oddness | (j << 14) | ((position/2) << 8);
		spi_master_transfer(&temp, 4, 1);
		temp &= 0x000000FF;
		if(temp != write_buff[position])
			return -1;
	}
	/* Whatever is left over as less than a page */
	if(leftover)
	{
		for(i = 0; i < leftover; i++)
		{
			if(i % 2)
				oddness = 0x08000000;
			else
				oddness = 0;
			if(write_buff[i] != 0xFF)
				position = i;
			temp = LOAD_PAGE_BYTE | oddness | ((i/2) << 8) | (uint32_t)write_buff[i];
			spi_master_transfer(&temp, 4, 1);			
		}
	}
	if(position % 2)
		oddness = 0x08000000;
	else
		oddness = 0;
	temp = READ_PROG_MEM | oddness | (j << 14) | ((position/2) << 8);
	spi_master_transfer(&temp, 4, 1);
	temp &= 0x000000FF;
	if(temp != write_buff[position])
		return -1;
	
	return 1;
}

/************************************************************************/
/* CLEAR_WRITE_BUFF		                                                */
/* 																		*/
/* @purpose: Clears the array write_buff[]								*/
/************************************************************************/
void clear_write_buff(void)
{
	uint8_t i;
	for(i = 0; i < 128; i++)
	{
		write_buff[i] = 0;
	}
	return;
}
