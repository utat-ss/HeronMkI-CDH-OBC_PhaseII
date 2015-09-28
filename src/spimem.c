/*
	Author: Keenan Burnett

	***********************************************************************
*	FILE NAME:		spimem.c
*
*	PURPOSE:		This file is to be used for initializing and communicating with our 
*					SPI Memory chips. Part Number: S25FL208K0RMFI041.
*
*	FILE REFERENCES:		spimem.h
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
*	09/27/2015			Created
*
*	DESCRIPTION:
*
*			spimem_initialize gets the chip ready for communication with the OBC.
*
*			The remainder of the functions are fairly self-explanatory.
*
*/

#include "spimem.h"

/************************************************************************/
/*                                                                      */
/************************************************************************/
void spimem_initialize(void)
{
	uint32_t dumbuf;
	
	gpio_set_pin_high(SPI0_MEM2_HOLD);	// Turn "holding" off.
	gpio_set_pin_high(SPI0_MEM2_WP);	// Turn write protection off.
	
	dumbuf = WREN;
	spi_master_transfer(&dumbuf, 1, 2);
}

/************************************************************************/
/*                                                                      */
/************************************************************************/

// Note addresses on these SPI Memory chips are 24 bits.
// Returns -1 if the request failed, 
// otherwise returns the number of consecutive bytes which were successfully written to memory.
uint32_t spimem_write(uint8_t spi_chip, uint32_t addr, uint32_t* data_buff, uint32_t size)
{
	uint32_t i, size1, size2, low;
	uint16_t msg_buff[size + 5];
	msg_buff[0] = WREN;
	msg_buff[1] = PP;
	
	if (size > 256)				// Invalid size to write.
		return -1;
	if (addr > 0xFFFFF)			// Invalid address to write to.
		return -1;				
	
	low = addr & 0x000000FF;
	if ((size + low) > 256)		// Requested write flows into a second page.
	{
		size1 = 256 - low;
		size2 = size - size1;
	}
	else
	{
		size1 = size;
		size2 = 0;
	}
	if (addr + (size - 1) > 0xFFFFF)	// Address too high, can't write all requested bytes.
	{
		size1 = 256 - low;
		size2 = 0;
	}
	
	msg_buff[2] = addr & 0x000F0000;
	msg_buff[3] = addr & 0x0000FF00;
	msg_buff[4] = addr & 0x000000FF;
	for (i = 5; i < (size1 + 5); i++)
	{
		msg_buff[i] = *(data_buff + (i - 5));
	}
	/* Transfer commands and data to the memory chip */
	spi_master_transfer(&msg_buff, size, spi_chip);
	delay_ms(10);			// Internal Write Time for the SPI Memory Chip.
	
	if(size2)	// Requested write flows into a second page.
	{
		msg_buff[0] = WREN;
		msg_buff[1] = PP;
		msg_buff[2] = (addr + size1) & 0x000F0000;
		msg_buff[3] = (addr + size1) & 0x0000FF00;
		msg_buff[4] = (addr + size1) & 0x000000FF;
		for (i = 5; i < (size2 + 5); i++)
		{
			msg_buff[i] = *(data_buff + ((i + size1) - 5));
		}
		/* Transfer commands and data to the memory chip */
		spi_master_transfer(&msg_buff, size, spi_chip);
		delay_ms(10);			// Internal Write Time for the SPI Memory Chip.
	}
	
	return (size1 + size2);
}

uint32_t check_page(uint32_t page_num)
{
	uint32_t byte_offset, integer_number;
	if (page_num > 0xFFF)					// Invalid Page Number
	return  -1;
	byte_offset = page_num % 32;
	integer_number = page_num / 32;
	
	return spi_bit_map[integer_number] & (1 << byte_offset);
}

uint32_t set_page_dirty(uint32_t page_num)
{	
	uint32_t byte_offset, integer_number;
	if (page_num > 0xFFF)					// Invalid Page Number
		return  -1;
	byte_offset = page_num % 32;
	integer_number = page_num / 32;
	
	spi_bit_map[integer_number] |= (1 << byte_offset);
	return 0;
}

uint32_t set_sector_clean(uint32_t sect_num)
{
	uint32_t page_num, integer_number;
	// 1 Sect == 16 Pages
	// 16 Pages == 0.5 Integers w/in BitMap
	if(sect_num > 0xFF)						// Invalid Sector Number
		return -1;
	
	page_num = sect_num * 16;
	integer_number = page_num / 32;
	
	if((page_num % 2) == 0)
	{
		spi_bit_map[integer_number] &= 0xFF00;
	}
	else
		spi_bit_map[integer_number] &= 0x00FF;
	
	return 0;
}

