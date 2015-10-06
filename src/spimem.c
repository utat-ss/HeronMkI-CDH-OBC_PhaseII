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
*	NOTES:		We are going to have 3 1MB SPI Memory chips, 2 of which will be redundant.
*
*				A page on these SPI chips is 256 bytes, a sector is 4kB, and a block is 64kB.
*
*				A write can be as long as 256 Bytes because I don't want a single process hogging
*				the SPI Memory for too long.
*
*				A write operation can turn 1s to 0s ONLY. Hence if you want to write to a previously
*				written area, you need to erase it first.
*
*				Unfortunately, erase operations do 4kB as a MINIMUM and can take up to 300 ms.
*
*				In order to mitigate this erasing overhead, we will be implementing a BITMAP.
*
*				This BITMAP will allow us to keep track of which pages in memory are "dirty" and will
*				therefore tell us whether we need to go through the pain of loading an entire sector
*				into memory and then writing it back after a sector-erase operation.
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
/* SPIMEM_INITIALIZE                                                    */
/* 																		*/
/* @param: None.														*/
/* @purpose: This function is used to initialize each SPI Chip so that	*/
/* they are ready for communication with the OBC. We also initialize 	*/
/* the bitmap and SPI Memory Buffer here.								*/
/* @return: None.														*/
/************************************************************************/

// NOTE: The initialization procedure needs to be repeated for each SPi Mem Chip.
void spimem_initialize(void)
{
	uint16_t dumbuf[1], i;
	uint8_t check;
	
	gpio_set_pin_low(SPI0_MEM1_HOLD);	// Turn "holding" off.
	gpio_set_pin_low(SPI0_MEM1_WP);	// Turn write protection off.
	gpio_set_pin_high(SPI0_MEM2_HOLD);	// Turn "holding" off.
	gpio_set_pin_high(SPI0_MEM2_WP);	// Turn write protection off.
	
	
	//dumbuf = CE;							// Chip-Erase (this operation can take up to 7s.
	//spi_master_transfer(&dumbuf, 1, 2);
	//delay_s(7);								// delay 7 seconds.
	
	//if(check_if_wip(1) != 1)				// Wait for the erase to finish (10 more ms allowed).
	//	return;								// FAILURE_RECOVERY required here.
	
	//check = get_spimem_status(2);
	//check = check & 0x03;
	
//	if(check != 0x02)
//		return;								// FAILURE_RECOVERY required here.
		
//	pio_toggle_pin(LED3_GPIO);
	
	while(1)
	{
		dumbuf[0] = WREN;						// Write-Enable Command.
		spi_master_transfer(dumbuf, 1, 2);
		check = get_spimem_status(2);
		if(check != 0x00)
			pio_toggle_pin(LED3_GPIO);
	}
	
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
	uint32_t i, size1, size2, low, dirty = 0, page, sect_num, check;
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
	if ((addr + (size - 1)) > 0xFFFFF)	// Address too high, can't write all requested bytes.
	{
		size1 = 256 - low;
		size2 = 0;
	}

	if(check_if_wip(spi_chip) != 1)		// SPI chip is either tied up or dead, return FAILURE.
		return -1;

	page = get_page(addr);
	dirty = check_page(page);
	if(dirty)
	{
		sect_num = get_sector(addr);
		check = load_sector_into_spibuffer(spi_chip, sect_num);			// if check != 4096, FAILURE_RECOVERY.
		check = update_spibuffer_with_new_page(addr, data_buff, size1);	// if check != size1, FAILURE_RECOVERY.
		check = erase_sector_on_chip(spi_chip, sect_num);				// FAILURE_RECOVERY
		check = write_sector_back_to_spimem(spi_chip);					// FAILURE_RECOVERY
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
		spi_master_transfer(&msg_buff, (size1 + 5), spi_chip);

		set_page_dirty(page);	// Page has been written to, set dirty.

		delay_ms(10);			// Internal Write Time for the SPI Memory Chip.		
	}
	
	if(size2)	// Requested write flows into a second page.
	{
		page = get_page(addr);
		dirty = check_page(page);
		if(dirty)
		{
			sect_num = get_sector(addr);
			check = load_sector_into_spibuffer(spi_chip, sect_num);						// if check != 4096, FAILURE_RECOVERY.
			check = update_spibuffer_with_new_page(addr, (data_buff + size1), size2);	// if check != size1, FAILURE_RECOVERY.
			check = erase_sector_on_chip(spi_chip, sect_num);				// FAILURE_RECOVERY
			check = write_sector_back_to_spimem(spi_chip);					// FAILURE_RECOVERY

		}
		else
		{
			msg_buff[0] = WREN;
			msg_buff[1] = PP;
			msg_buff[2] = (addr + size1) & 0x000F0000;
			msg_buff[3] = (addr + size1) & 0x0000FF00;
			msg_buff[4] = (addr + size1) & 0x000000FF;

			for (i = 5; i < (size2 + 5); i++)
			{
				msg_buff[i] = *((data_buff + size1) + (i - 5));
			}
			/* Transfer commands and data to the memory chip */
			spi_master_transfer(&msg_buff, (size2 + 5), spi_chip);

			set_page_dirty(page);	// Page has been written to, set dirty.

			delay_ms(10);			// Internal Write Time for the SPI Memory Chip.	
		}
	}
	
	return (size1 + size2);
}

/************************************************************************/
/* SPIMEM_READ 		                                                    */
/* 																		*/
/* @param: spi_chip: Indicates which SPI CHIP we are communicating with */
/* which is either 1, 2, or 3.											*/
/* @param: addr: indicates the address on the SPI_CHIP we would like to */
/* read from.															*/
/* @param: read_buff: Buffer in which the read bytes will be placed.	*/
/* @param: size: How many bytes we would like to read into memory.		*/
/* @return: -1 == Failure, otherwise returns the number of pages which	*/
/* were read into the buffer.											*/
/* @purpose: Read from the SPI memory chip.								*/
/************************************************************************/
uint32_t spimem_read(uint32_t spi_chip, uint32_t addr, uint8_t* read_buff, uint32_t size)
{
	uint16_t dumbuf[4];

	if (addr > 0xFFFFF)			// Invalid address to write to.
		return -1;
	if ((addr + size - 1) > 0xFFFFF)
		return -1;

	dumbuf[0] = RD;
	dumbuf[1] = addr & 0x00FF0000;
	dumbuf[2] = addr & 0x0000FF00;
	dumbuf[3] = addr & 0x000000FF;

	// Make sure that WIP == 0.

	spi_master_transfer(&dumbuf, 4, spi_chip);

	spi_master_read(read_buff, size, spi_chip);

	return size;
}

/************************************************************************/
/* LOAD_SECTOR_INTO_SPIBUFFER                                           */
/* 																		*/
/* @param: spi_chip: Indicates which SPI CHIP we are communicating with */
/* which is either 1, 2, or 3.											*/
/* @param: sect_num: indicates the sector we would like to load into mem*/
/* @return: -1 == Failure, otherwise returns the number of bytes which  */
/* were successfully read into the SPI BUFFER.							*/
/* @purpose: The function is to be used immediately before a sector		*/
/* -erase operation. Loads 4kB from SPI_CHIP into spi_mem_buffer		*/
/************************************************************************/
uint32_t load_sector_into_spibuffer(uint32_t spi_chip, uint32_t sect_num)
{
	uint32_t addr, read;

	if(sect_num > 255)				// Invalid sector to request a write to.
		return -1;

	addr = sect_num << 12;

	read = spimem_read(spi_chip, addr, spi_mem_buff, 4096);
	spi_mem_buff_sect_num = sect_num;

	return read;
}

/************************************************************************/
/* UPDATE_SPIBUFFER_WITH_NEW_PAGE                                       */
/* 																		*/
/* @param: addr: Exactly the same as the address that we intend to write*/
/* to the SPI memory chip.												*/
/* @param: *data_buff: Contains the data to be written to memory.		*/
/* @param: size: How many bytes to write to memory.						*/
/* @return: -1 == Failure, 1 == Success.								*/
/* @purpose: The function is to be used immediately after a sector has	*/
/* been loaded into memory so as to update it with the new page write. 	*/
/* @NOTE: addr must correpond to an address within the sector which is 	*/
/* currently loaded into memory.										*/
/* @NOTE: even though this is a 16-bit variable, we are only using the 	*/
/* lower 8 bits.														*/
/************************************************************************/
uint32_t update_spibuffer_with_new_page(uint32_t addr, uint32_t* data_buff, uint32_t size)
{
	uint32_t sect_num, page_num, index, i;

	page_num = get_page(addr);
	sect_num = page_num << 4;		// Get the sector number that we wish to write to.
	index = addr & 0x00000FFF;

	if(sect_num != spi_mem_buff_sect_num)	// The desired page write does not belong to the sector currently in memory.
		return -1;

	for (i = 0; i < size; i++)
	{
		spi_mem_buff[index + i] = (uint8_t)(*(data_buff + i));
	}

	return 1;
}


/************************************************************************/
/* CHECK_PAGE 				                                            */
/* 																		*/
/* @param: page_num: The page that we would like to see the status of.	*/
/* @purpose: This function either returns 1 or 0 to indicate a dirty or */
/* a clean page respectively.											*/
/************************************************************************/
uint32_t check_page(uint32_t page_num)
{
	uint32_t byte_offset, integer_number;
	if (page_num > 0xFFF)					// Invalid Page Number
	return  -1;
	byte_offset = page_num % 32;
	integer_number = page_num / 32;
	
	return (spi_bit_map[integer_number] & (1 << byte_offset)) >> byte_offset;
}

/************************************************************************/
/* GET_PAGE 				                                            */
/* 																		*/
/* @param: addr: unaltered 24-bit address to the SPI-CHIP.				*/
/* @purpose: This function returns the page number of the address which	*/
/* was requested.														*/
/************************************************************************/
uint32_t get_page(uint32_t addr)
{
	return (addr >> 8);
}

/************************************************************************/
/* GET_SECTOR 				                                            */
/* 																		*/
/* @param: addr: unaltered 24-bit address to the SPI-CHIP.				*/
/* @purpose: This function returns the sector number of the address 	*/
/* which was requested.													*/
/************************************************************************/
uint32_t get_sector(uint32_t addr)
{
	return (addr >> 12);
}

/************************************************************************/
/* GET_SPIMEM_STATUS 		                                            */
/* 																		*/
/* @param: spi_chip: Indicates which SPI CHIP we are communicating with */
/* which is either 1, 2, or 3.											*/
/* @purpose: This function returns the status register from the SPI 	*/
/* memory chip that was requested.										*/
/************************************************************************/
uint8_t get_spimem_status(uint32_t spi_chip)
{
	uint16_t dumbuf[2];
	dumbuf[0] = RSR;						// Read Status Register.
	dumbuf[1] = 0x00;
	spi_master_transfer(dumbuf, 2, spi_chip);

	return (uint8_t)dumbuf[1];				// Status of the Chip is returned.
}

/************************************************************************/
/* SET_PAGE_DIRTY 			                                            */
/* 																		*/
/* @param: page_num: The page which we would like to set as "DIRTY" == 1*/
/* within the bitmap.													*/
/* @return: -1 == Failure, 1 == Success.								*/
/* @purpose: This function is used to set a page "DIRTY" within BitMap  */
/************************************************************************/
uint32_t set_page_dirty(uint32_t page_num)
{	
	uint32_t byte_offset, integer_number;
	if (page_num > 0xFFF)					// Invalid Page Number
		return  -1;
	byte_offset = page_num % 32;
	integer_number = page_num / 32;
	
	spi_bit_map[integer_number] |= (1 << byte_offset);
	return 1;
}

/************************************************************************/
/* SET_SECTOR_CLEAN_IN_BITMAP                                           */
/* 																		*/
/* @param: sect_num: The sector which we would like to set as "clean"	*/
/* in the bitmap.														*/
/* @return: -1 == Failure, 1 == Success.								*/
/* @purpose: This function is used to set an entire "sector" = 16 Pages */
/* as "CLEAN" within the bitmap which is intended to directly reflect	*/
/* the state of memory with in the SPI CHIPS.							*/
/************************************************************************/

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
	
	return 1;
}

/************************************************************************/
/* ERASE_SECTOR_ON_CHIP		                                            */
/* 																		*/
/* @param: spi_chip: Indicates which SPI CHIP we are communicating with */
/* which is either 1, 2, or 3.											*/
/* @param: sect_num: The sector which we would like to physically erase */
/* on the SPI memory chip.												*/
/* @return: -1 == Failure, 1 == Success									*/
/* @purpose: The function is to be used to erase a single sector within	*/
/* a SPI memory chip.													*/
/************************************************************************/
uint32_t erase_sector_on_chip(uint32_t spi_chip, uint32_t sect_num)
{
	uint16_t msg_buff[5];
	uint8_t addr;

	if(sect_num > 0xFF)							// Invalid Sector Number
		return -1;

	if(check_if_wip(spi_chip) != 1)
		return -1;								// SPI_CHIP is currently tied up or dead, return FAILURE.
		
	addr = (uint8_t)(sect_num << 12);

	msg_buff[0] = WREN;
	msg_buff[1] = SE;
	msg_buff[2] = addr & 0x000F0000;
	msg_buff[3] = addr & 0x0000FF00;
	msg_buff[4] = addr & 0x000000FF;

	spi_master_transfer(&msg_buff, 5, spi_chip);

	return 1;									// Erase Operation Succeeded.
}

/************************************************************************/
/* WRITE_SECTOR_BACK_TO_SPIMEM		                                    */
/* 																		*/
/* @param: spi_chip: Indicates which SPI CHIP we are communicating with */
/* which is either 1, 2, or 3.											*/
/* @return: -1 == Failure, Otherwise this function returns the Number 	*/
/* of bytes which were successfully written to the SPI Chip.			*/
/* @purpose: The function is to be usd to load the entire spi_mem_buff  */
/* back into memory. (The typical sequence is load-erase-writeback      */
/************************************************************************/
uint32_t write_sector_back_to_spimem(uint32_t spi_chip)
{
	uint16_t msg_buff[271];
	uint32_t addr, i, j;

	if(check_if_wip(spi_chip) != 1)
		return -1;									// SPI_CHIP is currently tied up or dead, return FAILURE.

	addr = spi_mem_buff_sect_num << 12;

	for (i = 0; i < 16; i++)						// Write the Buffer back, 1 page at a time.
	{
		msg_buff[0] = WREN;
		msg_buff[1] = PP;
		msg_buff[2] = (addr + 256 * i) & 0x000F0000;
		msg_buff[3] = (addr + 256 * i) & 0x0000FF00;
		msg_buff[4] = (addr + 256 * i) & 0x000000FF;

		for (j = 5; j < 271; j++)
		{
			msg_buff[j] = spi_mem_buff[256 * i + j];
		}

		spi_master_transfer(&msg_buff, 271, spi_chip);

		if(check_if_wip(spi_chip) != 1)
			return i * 256;							// Write operation took too long, return number of bytes transferred.
	}

	return i * 256;									// Return the number of bytes which were written to SPI memory.
}

/************************************************************************/
/* CHECK_IF_WIP 														*/
/* 																		*/
/* @param: spi_chip: Indicates which SPI CHIP we are communicating with */
/* which is either 1, 2, or 3.											*/
/* @return: -1 == Failure, so a Write is Still "In-Progess", 1 == Succes*/
/* Hence, the SPI_CHIP is free to write to.								*/
/* @purpose: The function is to be used to check if a SPI MEM chip is 	*/
/* currently tied up with a write operation, this function will wait for*/
/* a maximum of 10ms for the chip to become free.(approximately the 	*/
/* length of a single page-write operation)								*/
/************************************************************************/
uint32_t check_if_wip(uint32_t spi_chip)
{
	uint8_t status;
	uint32_t timeout = 21053;						// ~10ms timeout.

	status = get_spimem_status(spi_chip);
	status = status & 0x03;
	while((status != 0x02) && (timeout--))			// Wait for WIP to be zero (Write-In-Progess)
	{
		status = get_spimem_status(spi_chip);
		status = status & 0x03;
	}

	if((status != 0x02) || !timeout)				// Waiting took too long, return FAILURE.
		return -1;
	else
		return 1;
}
