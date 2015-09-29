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

// NOTE: The initialization procedure needs to be repeated for each SPi Mem Chip.
void spimem_initialize(void)
{
	uint16_t dumbuf[2], i;
	uint8_t check;
	
	gpio_set_pin_high(SPI0_MEM2_HOLD);	// Turn "holding" off.
	gpio_set_pin_high(SPI0_MEM2_WP);	// Turn write protection off.
	
	dumbuf[0] = WREN;						// Write-Enable Command.
	spi_master_transfer(dumbuf, 1, 2);

	check = get_spimem_status(2);		// Get the status of the SPI Mem Chip.

	check = check & 0x03;

	if (check != 0x02)
		i = 0;							// FAILURE_RECOVERY required here.

	for (i = 0; i < 128; i++)
	{
		spi_bit_map[i] = 0;				// Initialize the bitmap
	}

	for (i = 0; i < 4096; i++)
	{
		spi_mem_buff[i] = 0;			// Initialize the memory buffer.
	}
	return;
}

/************************************************************************/
/* SPIMEM_WRITE                                                         */
/*																		*/
/* @param: spi_chip: This indicates which chip/chip_select you would	*/
/* like to communicate with. Ex:1, 2, or 3.								*/
/* @param: addr: This is address you intend to write to in SPI Memory 	*/
/* @param: *data_buff: Contains the data to be written to memory.		*/
/* @param: size: Length of the aforementioned buffer.					*/
/* @Note: addresses on these SPI Memory chips are 24 bits.				*/
/* @Return: Returns -1 if the request failed, otherwise returns the 	*/
/* number of consecutive bytes which were successfully written to memory*/
/* @Note: This function simply writes the bytes you ask for into 		*/
/* acsending order (numerically) in memory.								*/
/************************************************************************/
uint32_t spimem_write(uint8_t spi_chip, uint32_t addr, uint32_t* data_buff, uint32_t size)
{
	uint32_t i, size1, size2, low, dirty = 0, page;
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

	page = get_page(addr);
	dirty = check_page(page);
	if(dirty)
	{
		//load_sector_into_spibuffer(sect_num)
		//erase_sector(sect_num)
		//write_sector_back_to_spimem(*spi_mem_buffer)

	}
	else
	{
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
	}
	
	if(size2)	// Requested write flows into a second page.
	{
		page = get_page(addr);
		dirty = check_page(page);
		if(dirty)
		{
			//load_sector_into_spibuffer(sect_num)
			//erase_sector(sect_num)
			//write_sector_back_to_spimem(*spi_mem_buffer)

		}
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

uint32_t spimem_read(uint32_t spi_chip, uint32_t addr, uint16_t* read_buff, uint32_t size)
{
	uint16_t dumbuf[4];
	uint32_t i;

	if (addr > 0xFFFFF)			// Invalid address to write to.
		return -1;
	if ((addr + size - 1) > 0xFFFFF)
		return -1;

	dumbuf[0] = RD;
	dumbuf[1] = addr & 0x00FF0000;
	dumbuf[2] = addr & 0x0000FF00;
	dumbuf[3] = addr & 0x000000FF;

	// Make sure that WIP == 0.

	spi_master_transfer(&dumbuf, 1, spi_chip);

	spi_master_read(read_buff, size, spi_chip);

	return size;
}

uint32_t load_sector_into_spibuffer(uint32_t spi_chip, uint32_t sect_num)
{
	//
}

uint32_t check_page(uint32_t page_num)
{
	uint32_t byte_offset, integer_number;
	if (page_num > 0xFFF)					// Invalid Page Number
	return  -1;
	byte_offset = page_num % 32;
	integer_number = page_num / 32;
	
	return (spi_bit_map[integer_number] & (1 << byte_offset)) >> byte_offset;
}

uint32_t get_page(uint32_t addr)
{
	return (addr >> 8);
}

uint8_t get_spimem_status(uint32_t spi_chip)
{
	uint16_t dumbuf[2];
	dumbuf[0] = RSR;						// Read Status Register.
	dumbuf[1] = RSR;
	spi_master_transfer(dumbuf, 2, spi_chip);

	return (uint8_t)dumbuf[1];				// Status of the Chip is returned.
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

uint32_t set_sector_clean_in_bitmap(uint32_t sect_num)
{
	uint32_t page_num, integer_number;
	// 1 Sect == 16 Pages
	// 16 Pages == 0.5 Integers w/in BitMap
	if(sect_num > 0xFF)							// Invalid Sector Number
		return -1;
	
	page_num = sect_num * 16;
	integer_number = page_num / 32;
	
	if((page_num % 2) == 0)
	{
		spi_bit_map[integer_number] &= 0xFF00;	// Clear the lower 16 pages.
	}
	else
		spi_bit_map[integer_number] &= 0x00FF;	// Clear the upper 16 pages.
	
	return 0;
}

