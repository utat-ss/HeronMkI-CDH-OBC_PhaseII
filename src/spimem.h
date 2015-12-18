/*
	Author: Keenan Burnett

	***********************************************************************
*	FILE NAME:		spi_func.c
*
*	PURPOSE:		Houses the includes and definitions for spimem.c
*
*
*	FILE REFERENCES:		spi_func.h, gpio.h, time.h, FreeRTOS.h, task.h
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
#include "FreeRTOS.h"
#include "semphr.h"
#include "atomic.h"
#include "global_var.h"

SemaphoreHandle_t	Spi0_Mutex;

/* Determines whether or not the spimem chips are cleared upon a reset */
#define ERASE_SPIMEM_ON_RESET 1

/*		SPI MEMORY COMMANDS		*/
#define		WREN	0x06		// Write-Enable
#define		WRDI	0x04		// Write-Disable
#define		WSR		0x01		// Write Status Register
#define		RSR		0x05		// Read Status Register
#define		RD		0x03		// Read Data
#define		PP		0x02		// Page Programming
#define		SE		0x20		// Sector Erase (4kB)
#define		CE		0xC7		// Chip Erase

/* SPI MEMORY BASE ADDRESSES	*/
#define		COMS_BASE		0x00000		// COMS = 16kB: 0x00000 - 0x03FFF
#define		EPS_BASE		0x04000		// EPS = 16kB: 0x04000 - 0x07FFF
#define		PAY_BASE		0x08000		// PAY = 16kB: 0x08000 - 0x0BFFF
#define		HK_BASE			0x0C000		// HK = 8kB: 0x0C000 - 0x0DFFF
#define		EVENT_BASE		0x0E000		// EVENT = 8kB: 0x0E000 - 0x0FFFF
#define		SCHEDULE_BASE	0x10000		// SCHEDULE = 8kB: 0x10000 - 0x11FFF
#define		TIME_BASE		0xFFFFC		// TIME = 4B: 0xFFFFC - 0xFFFFF

/*		Global Variable Definitions		*/
uint32_t spi_bit_map[128];		// Bit-Map to pages (256B) within SPI Memory.
uint8_t	spi_mem_buff[4096];		// Buffer required when erasing a sector
uint32_t spi_mem_buff_sect_num;	// Current sector number of what is loaded into the SPI Memory Buffer.
uint16_t msg_buff[260];			// Temporary buffer used by the read and write tasks to store data.

/*		Function Prototypes				*/
void spimem_initialize(void);																	// Driver
int spimem_write(uint32_t addr, uint8_t* data_buff, uint32_t size);								// API, BLOCKS FOR 3 TICK
int spimem_write_h(uint8_t spi_chip, uint32_t addr, uint8_t* data_buff, uint32_t size);			// API, BLOCKS FOR 1 TICK
int spimem_read(uint32_t addr, uint8_t* read_buff, uint32_t size);								// API, BLOCKS FOR 1 TICK
int spimem_read_alt(uint32_t spi_chip, uint32_t addr, uint8_t* read_buff, uint32_t size);		// API, BLOCKS FOR 1 TICK
uint32_t check_page(uint32_t page_num);															// Helper
uint32_t check_if_wip(uint32_t spi_chip);														// Helper
uint32_t get_page(uint32_t addr);																// Helper
uint32_t get_sector(uint32_t addr);																// Helper
int get_spimem_status(uint32_t spi_chip);														// API, BLOCKS FOR 1 TICK
uint32_t set_page_dirty(uint32_t page_num);														// Helper
uint32_t set_sector_clean_in_bitmap(uint32_t sect_num);											// Helper
uint32_t load_sector_into_spibuffer(uint32_t spi_chip, uint32_t sect_num);						// Driver
uint32_t update_spibuffer_with_new_page(uint32_t addr, uint8_t* data_buff, uint32_t size);		// Driver
uint32_t erase_sector_on_chip(uint32_t spi_chip, uint32_t sect_num);							// Driver
uint32_t write_sector_back_to_spimem(uint32_t spi_chip);										// Driver
int erase_spimem(void)																			// Helper

