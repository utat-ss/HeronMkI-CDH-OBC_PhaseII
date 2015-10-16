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
*	10/05/2015			I have verified that I am able to communicate with the SPI chip by retrieving its status byte.
*
*	10/07/2015			The code here appears to be mostly complete, I have added in the functionality so that APIs
*						must acquire the Spi0_Mutex before they can complete their operation. Each of these functions
*						will only block for 1 tick before returning a failure status. 
*
*						I am currently writing a test program in order to verify the functionality of all this code.
*
*	10/13/2015			I have tested this code against my test program spimemtest.c (located in the test repository)
*						and debugged any defects which came up during the testing. So as far as I know, this set of
*						drivers and APIs are good to go for future usage.
*
*	10/16/2015			It occurred to me yesterday that certain operations that our satellite performs (such as
*						spi_mem_read and spi_mem_write need to be atomic (execute uninterrupted). Hence I am 
*						making a simple API for other space systems to use. It will also be used in the functions
*						I mentioned above.
*
*	DESCRIPTION:
*
*			spimem_initialize gets the chip ready for communication with the OBC.
*
*			The remainder of the functions are fairly self-explanatory if you read their headers.
*
*/

#include "spimem.h"

static uint8_t get_spimem_status_h(uint32_t spi_chip);
static uint32_t write_page_h(uint8_t spi_chip, uint32_t addr, uint8_t* data_buff, uint32_t size);
static uint32_t ready_for_command_h(uint32_t spi_chip);

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
	uint16_t dumbuf[2], i;
	uint8_t check;
	uint32_t timeout = 1500;		// ~15s timeout
	
	gpio_set_pin_low(SPI0_MEM1_HOLD);	// Turn "holding" off.
	gpio_set_pin_low(SPI0_MEM1_WP);	// Turn write protection off.
	gpio_set_pin_high(SPI0_MEM2_HOLD);	// Turn "holding" off.
	gpio_set_pin_high(SPI0_MEM2_WP);	// Turn write protection off.
	
	if(ready_for_command_h(2) != 1)				// Check if the chip is ready to receive commands.
		return;									// FAILURE_RECOVERY.
		
	dumbuf[0] = CE;							// Chip-Erase (this operation can take up to 7s.
	spi_master_transfer(dumbuf, 1, 2);
	
	while((check_if_wip(2) != 0) && timeout--){ }	// Wait for a maximum of 15 s.
		
	if((check_if_wip(2) != 0) || !timeout)
		return ;							// FAILURE_RECOVERY
		
	if(ready_for_command_h(2) != 1)
		return;							// FAILURE_RECOVERY
	
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
/* ascending order (numerically) in memory.								*/
/* @NOTE: This function first attempts to acquire the mutex for SPI0	*/
/* it will block for a maximum of 1 Tick, if SPI0 is still occupied		*/
/* after that, the function returns -1.									*/
/* @Note: This function is an atomic operation and hence suspends		*/
/* interrupts temporarily.												*/
/************************************************************************/
int spimem_write(uint8_t spi_chip, uint32_t addr, uint8_t* data_buff, uint32_t size)
{
	uint32_t size1, size2, low, dirty = 0, page, sect_num, check;
		
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

	if (xSemaphoreTake(Spi0_Mutex, (TickType_t) 1) == pdTRUE)	// Only Block for a single tick.
	{
		enter_atomic();											// Atomic operation begins.
		
		if(ready_for_command_h(spi_chip) != 1)
		{
			exit_atomic();
			xSemaphoreGive(Spi0_Mutex);
			return -1;
		}
					
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
			if(write_page_h(spi_chip, addr, data_buff, size1) != 1)
			{
				exit_atomic();						// Atomic operation ends.
				xSemaphoreGive(Spi0_Mutex);
				return -1;
			}
		}
		if(size2)	// Requested write flows into a second page.
		{
			page = get_page(addr + size1);
			dirty = check_page(page);
			
			if(ready_for_command_h(spi_chip) != 1)
			{
				exit_atomic();
				xSemaphoreGive(Spi0_Mutex);
				return size1;
			}
			if(dirty)
			{
				sect_num = get_sector(addr + size1);
				check = load_sector_into_spibuffer(spi_chip, sect_num);						// if check != 4096, FAILURE_RECOVERY.
				check = update_spibuffer_with_new_page(addr + size1, (data_buff + size1), size2);	// if check != size1, FAILURE_RECOVERY.
				check = erase_sector_on_chip(spi_chip, sect_num);				// FAILURE_RECOVERY
				check = write_sector_back_to_spimem(spi_chip);					// FAILURE_RECOVERY

			}
			else
			{		
				if(write_page_h(spi_chip, addr + size1, (data_buff + size1), size2) != 1)
				{
					exit_atomic();
					xSemaphoreGive(Spi0_Mutex);
					return size1;
				}
			}
		}
		
		exit_atomic();
		xSemaphoreGive(Spi0_Mutex);
		return (size1 + size2);	
	}

	else
		return -1;												// SPI0 is currently being used or there is an error.
}

/************************************************************************/
/* SPIMEM_READ_H 		                                                    */
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
/* @NOTE: This function is a helper and is ONLY to be used within a 	*/
/* section of code which has acquired the Spi0_Mutex.					*/
/************************************************************************/
static int spimem_read_h(uint32_t spi_chip, uint32_t addr, uint8_t* read_buff, uint32_t size)
{
	uint32_t i;

	if (addr > 0xFFFFF)										// Invalid address to write to.
		return -1;
	if ((addr + size - 1) > 0xFFFFF)
		size = 0xFFFFF - size;

	msg_buff[0] = RD;
	msg_buff[1] = (uint16_t)((addr & 0x000F0000) >> 16);
	msg_buff[2] = (uint16_t)((addr & 0x0000FF00) >> 8);
	msg_buff[3] = (uint16_t)(addr & 0x000000FF);
		
	for(i = 4; i < 260; i++)
	{
		msg_buff[i] = 0;
	}

	if(check_if_wip(spi_chip) != 0)							// A write is still in effect, FAILURE_RECOVERY.
		return -1;

	spi_master_transfer(msg_buff, 260, spi_chip);	// Keeps CS low so that read may begin immediately.

	for(i = 4; i < 260; i++)
	{
		*(read_buff + (i - 4)) = (uint8_t)msg_buff[i];
	}

	return size;
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
/* @NOTE: This function first attempts to acquire the mutex for SPI0	*/
/* it will block for a maximum of 1 Tick, if SPI0 is still occupied		*/
/* after that, the function returns -1.									*/
/************************************************************************/
int spimem_read(uint32_t spi_chip, uint32_t addr, uint8_t* read_buff, uint32_t size)
{
	uint32_t i;
	
	if (addr > 0xFFFFF)										// Invalid address to write to.
		return -1;
	if ((addr + size - 1) > 0xFFFFF)						// Read would overflow highest address, read less.
		size = 0xFFFFF - size;

	if (xSemaphoreTake(Spi0_Mutex, (TickType_t) 1) == pdTRUE)	// Only Block for a single tick.
	{
		enter_atomic();											// Atomic operation begins.
		
		msg_buff[0] = RD;
		msg_buff[1] = (uint16_t)((addr & 0x000F0000) >> 16);
		msg_buff[2] = (uint16_t)((addr & 0x0000FF00) >> 8);
		msg_buff[3] = (uint16_t)(addr & 0x000000FF);
		
		for(i = 4; i < 260; i++)
		{
			msg_buff[i] = 0;
		}

		if(check_if_wip(spi_chip) != 0)							// A write is still in effect, FAILURE_RECOVERY.
		{
			exit_atomic();
			xSemaphoreGive(Spi0_Mutex);
			return -1;
		}
		
		spi_master_transfer(msg_buff, 260, spi_chip);	// Keeps CS low so that read may begin immediately.

		for(i = 4; i < 260; i++)
		{
			*(read_buff + (i - 4)) = (uint8_t)msg_buff[i];
		}

		exit_atomic();
		xSemaphoreGive(Spi0_Mutex);
		return size;
	}
	else
		return -1;												// SPI0 is currently being used or there is an error.
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
/* @NOTE: This function is a helper and is ONLY to be used within a 	*/
/* section of code which has acquired the Spi0_Mutex.					*/
/* @Note: This function is an atomic operation and hence suspends		*/
/* interrupts temporarily.												*/
/************************************************************************/
uint32_t load_sector_into_spibuffer(uint32_t spi_chip, uint32_t sect_num)
{
	uint32_t addr, read, temp, i;

	if(sect_num > 255)				// Invalid sector to request a write to.
		return -1;

	addr = sect_num << 12;

	for(i = 0; i < 16; i++)
	{
		temp = spimem_read_h(spi_chip, (addr + i * 256), (spi_mem_buff + i * 256), 256);
		if(temp == -1)
			return i * 256;
		read += temp;
	}

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
uint32_t update_spibuffer_with_new_page(uint32_t addr, uint8_t* data_buff, uint32_t size)
{
	uint32_t sect_num, page_num, index, i;

	page_num = get_page(addr);
	sect_num = page_num << 4;		// Get the sector number that we wish to write to.
	index = addr & 0x00000FFF;

	if(sect_num != spi_mem_buff_sect_num)	// The desired page write does not belong to the sector currently in memory.
		return -1;

	for (i = 0; i < size; i++)
	{
		spi_mem_buff[index + i] = *(data_buff + i);
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
/* GET_SPIMEM_STATUS_H 		                                            */
/* 																		*/
/* @param: spi_chip: Indicates which SPI CHIP we are communicating with */
/* which is either 1, 2, or 3.											*/
/* @purpose: This function returns the status register from the SPI 	*/
/* memory chip that was requested.										*/
/* @NOTE: This function is a helper and is ONLY to be used within a 	*/
/* section of code which has acquired the Spi0_Mutex.					*/
/************************************************************************/
static uint8_t get_spimem_status_h(uint32_t spi_chip)
{
	uint16_t dumbuf[2];
	dumbuf[0] = RSR;											// Read Status Register.
	dumbuf[1] = 0x00;

	spi_master_transfer(dumbuf, 2, (uint8_t)spi_chip);
	return (uint8_t)dumbuf[1];						// Status of the Chip is returned.
}

/************************************************************************/
/* GET_SPIMEM_STATUS 		                                            */
/* 																		*/
/* @param: spi_chip: Indicates which SPI CHIP we are communicating with */
/* which is either 1, 2, or 3.											*/
/* @purpose: This function returns the status register from the SPI 	*/
/* memory chip that was requested.										*/
/* @NOTE: This function first attempts to acquire the mutex for SPI0	*/
/* it will block for a maximum of 1 Tick, if SPI0 is still occupied		*/
/* after that, the funciton returns -1.									*/
/************************************************************************/
int get_spimem_status(uint32_t spi_chip)
{
	uint16_t dumbuf[2];
	dumbuf[0] = RSR;											// Read Status Register.
	dumbuf[1] = 0x00;
	if (xSemaphoreTake(Spi0_Mutex, (TickType_t) 1) == pdTRUE)	// Only Block for a single tick.
	{
		enter_atomic();
		spi_master_transfer(dumbuf, 2, (uint8_t)spi_chip);
		xSemaphoreGive(Spi0_Mutex);
		return (uint8_t)dumbuf[1];						// Status of the Chip is returned.
		exit_atomic();
	}
	else
		return -1;										// SPI0 is currently being used or there is an error.
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
/* @NOTE: This function is a helper and is ONLY to be used within a 	*/
/* section of code which has acquired the Spi0_Mutex.					*/
/* @NOTE: If the time taken to execute this function is longer than		*/
/* 300ms, this function returns -1 (FAILURE).							*/
/************************************************************************/
uint32_t erase_sector_on_chip(uint32_t spi_chip, uint32_t sect_num)
{
	uint32_t addr, timeout = 30;

	if(sect_num > 0xFF)							// Invalid Sector Number
		return -1;

	if(ready_for_command_h(spi_chip) != 1)		// Check is chip is ready for a command.
		return -1;
		
	addr = sect_num << 12;

	msg_buff[0] = SE;
	msg_buff[1] = (uint8_t)((addr & 0x000F0000) >> 16);
	msg_buff[2] = (uint8_t)((addr & 0x0000FF00) >> 8);
	msg_buff[3] = (uint8_t)(addr & 0x000000FF);

	spi_master_transfer(msg_buff, 4, spi_chip);
	
	while((check_if_wip(spi_chip) != 0) && timeout--){ }	// Wait for a maximum of 300ms.
		
	if((check_if_wip(spi_chip) != 0) || !timeout)
		return -1;								// The Operation took too long.

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
/* @NOTE: This function is a helper and is ONLY to be used within a 	*/
/* section of code which has acquired the Spi0_Mutex.					*/
/************************************************************************/
uint32_t write_sector_back_to_spimem(uint32_t spi_chip)
{
	uint32_t addr, i, j;

	if(ready_for_command_h(spi_chip) != 1)		// Check is chip is ready for a command.
		return -1;

	addr = spi_mem_buff_sect_num << 12;

	for (i = 0; i < 16; i++)						// Write the Buffer back, 1 page at a time.
	{
		msg_buff[0] = WREN;
		spi_master_transfer(msg_buff,1, spi_chip);		

		msg_buff[0] = PP;
		msg_buff[1] = (uint16_t)(((addr + 256 * i) & 0x000F0000) >> 16);
		msg_buff[2] = (uint16_t)(((addr + 256 * i) & 0x0000FF00) >> 8);
		msg_buff[3] = (uint16_t)((addr + 256 * i) & 0x000000FF);

		for (j = 4; j < 260; j++)
		{
			msg_buff[j] = spi_mem_buff[256 * i + (j - 4)];
		}

		spi_master_transfer(msg_buff, 260, spi_chip);

		if(check_if_wip(spi_chip) != 0)
			return i * 256;							// Write operation took too long, return number of bytes transferred.								
	}
	return i * 256;								    // Return the number of bytes which were written to SPI memory.							
}

/************************************************************************/
/* CHECK_IF_WIP 														*/
/* 																		*/
/* @param: spi_chip: Indicates which SPI CHIP we are communicating with */
/* which is either 1, 2, or 3.											*/
/* @return: -1 == Failure, so a Write is Still "In-Progess", 1 = WIP	*/
/* 0 == Chip is Free.													*/
/* @purpose: The function is to be used to check if a SPI MEM chip is 	*/
/* currently tied up with a write operation, this function will wait for*/
/* a maximum of 10ms for the chip to become free.(approximately the 	*/
/* length of a single page-write operation)								*/
/* @NOTE: This function is ONLY to be used during SPIMEM initialization	*/
/* or when the SPI0 Mutex has already been acquired.					*/
/* @NOTE: If the operation takes longer than 5ms, this function returns */
/* -1 to indicate that a failure occurred.								*/
/************************************************************************/
uint32_t check_if_wip(uint32_t spi_chip)
{
	uint8_t status;
	uint32_t timeout = 50;						// ~5ms timeout.
	uint32_t ret_val = -1;;

	status = get_spimem_status_h(spi_chip);
	status = status & 0x01;
	
	while((status != 0x00) && (timeout--))			// Wait for WIP to be zero (Write-In-Progess)
	{
		status = get_spimem_status_h(spi_chip);
		status = status & 0x01;
		delay_us(100);
	}
	
	if((status == 0x01) || (!timeout))				// Waiting took too long, return FAILURE.
		return ret_val;
		
	if(status == 0x00)								// No Write in Progress, return 0.
	{
		ret_val = 0;
		return ret_val;
	}
	else
		return ret_val;
}

/************************************************************************/
/* WRITE_PAGE_H                                                         */
/*																		*/
/* @param: spi_chip: This indicates which chip/chip_select you would	*/
/* like to communicate with. Ex:1, 2, or 3.								*/
/* @param: addr: This is address you intend to write to in SPI Memory 	*/
/* @param: *data_buff: Contains the data to be written to memory.		*/
/* @param: size: Length of the aforementioned buffer.					*/
/* @Note: addresses on these SPI Memory chips are 24 bits.				*/
/* @Return: Returns -1 if the request failed.							*/
/* @Note: This is a helper function and as such it should only be used	*/
/* within a section of code which as acquired the SPI0 Mutex.			*/
/* @NOTE: This helper will only accept page-aligned addresses.			*/
/************************************************************************/
static uint32_t write_page_h(uint8_t spi_chip, uint32_t addr, uint8_t* data_buff, uint32_t size)
{			
	uint32_t i;
	
	msg_buff[0] = PP;
	msg_buff[1] = (uint16_t)((addr & 0x000F0000) >> 16);
	msg_buff[2] = (uint16_t)((addr & 0x0000FF00) >> 8);
	msg_buff[3] = (uint16_t)(addr & 0x000000FF);

	for (i = 4; i < (size + 4); i++)
	{
		msg_buff[i] = (uint16_t)(*(data_buff + (i - 4)));
	}

	/* Transfer commands and data to the memory chip */
	spi_master_transfer(msg_buff, (size + 4), spi_chip);

	set_page_dirty(get_page(addr));	// Page has been written to, set dirty.

	if(check_if_wip(spi_chip) != 0)
		return -1;
		
	return 1;
}


/************************************************************************/
/* READY_FOR_COMMAND                                                    */
/*																		*/
/* @param: spi_chip: This indicates which chip/chip_select you would	*/
/* like to communicate with. Ex:1, 2, or 3.								*/
/* @Return: Returns -1 if the request failed.							*/
/* @Note: This is a helper function and as such it should only be used	*/
/* within a section of code which as acquired the SPI0 Mutex.			*/
/* @Purpose: This function is used to ready a chip to receive a command */
/************************************************************************/
static uint32_t ready_for_command_h(uint32_t spi_chip)
{		
	uint32_t check;
	
	check = check_if_wip(spi_chip);
	if(check == 1)
		return -1;
			
	msg_buff[0] = WREN;										// Enable a Write.
	spi_master_transfer(msg_buff, 1, spi_chip);
	
	check = get_spimem_status_h(spi_chip);
	check = check & 0x03;
	
	if(check != 0x02)
		return -1;
		
	return 1;
}