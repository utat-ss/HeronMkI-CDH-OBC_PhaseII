/*
	Author: Keenan Burnett

	***********************************************************************
*	FILE NAME:		spi_func.c
*
*	PURPOSE:		Houses the includes and definitions for spimem.c
*
*
*	FILE REFERENCES:		spi_func.h, gpio.h
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
*			Why am I using a bitmap? When the SPI Memory chip executes a "write" operation,
*			it can change existing 1s to 0s, but not the other way around. Hence if you
*			desire to write a section of memory which has previously been written to,
*			you need to execute an erase operation on that region first. Unfortunately,
*			the smallest region that can be erased is a sector (4kB). Hence, if you want to
*			write to a "dirty" page then you need to erase the entire sector (takes ~100ms).
*			In order to avoid this overhead, we can use a "bitmap" which is an array that has
*			a 1 bit : 1 page correspondence with SPI memory, where 0 = "clean page" and 1 = 
*			"dirty page." That way we can reduce erasing overhead by only executing an erase
*			operation when we need to write to a dirty page.  
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
*	DEVELOPMENT HISTORY:
*	09/27/2015		Created
*
*/

#include "spi_func.h"
#include "gpio.h"
#include "time.h"

/*		SPI MEMORY COMMANDS		*/
#define		WREN	0x06		// Write-Enable
#define		WRDI	0x04		// Write-Disable
#define		WSR		0x01		// Write Status Register
#define		RSR		0x05		// Read Status Register
#define		RD		0x03		// Read Data
#define		PP		0x02		// Page Programming
#define		SE		0x20		// Sector Erase (4kB)
#define		CE		0x60		// Chip Erase

/*		Global Variable Definitions		*/
uint32_t spi_bit_map[128];		// Bit-Map to pages (256B) within SPI Memory.
uint8_t	spi_mem_buff[4096];		// Buffer required when erasing a sector 

/*		Fuction Prototypes				*/
void spimem_initialize(void);
uint32_t spimem_write(uint8_t spi_chip, uint32_t addr, uint32_t* data_buff, uint32_t size);
uint32_t spimem_read(uint32_t spi_chip, uint32_t addr, uint16_t* read_buff, uint32_t size);
uint32_t check_page(uint32_t page_num);
uint32_t get_page(uint32_t addr);
uint8_t get_spimem_status(uint32_t spi_chip);
uint32_t set_page_dirty(uint32_t page_num);
uint32_t set_sector_clean_in_bitmap(uint32_t sect_num);

