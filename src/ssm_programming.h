/*
	Author: Keenan Burnett

	***********************************************************************
*	FILE NAME:		ssm_programming.h
*
*	PURPOSE:		Includes, definitions, and prototypes for ssm_programming.c
*
*	FILE REFERENCES:		global_var.h, can_func.h, spi_func.h, gpio.h, time.h, FreeRTOS.h,
*							semphr.h, atomic.h
*
*	EXTERNAL VARIABLES:
*
*	EXTERNAL REFERENCES:	Same a File References.
*
*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
*
*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
*	DEVELOPMENT HISTORY:
*	01/12/2016			Created.
*
*/

#include "global_var.h"
#include "can_func.h"
#include "spi_func.h"
#include "gpio.h"
#include "time.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "atomic.h"
#include "spimem.h"

/* Command Definitions */
#define PROGRAM_ENABLE		0xAC530000
#define READ_SIGNATURE		0x30000000
#define CHIP_ERASE			0xAC800000
#define READ_PROG_MEM		0x20000000
#define LOAD_PAGE_BYTE		0x40000000
#define WRITE_PAGE			0x4C000000

/* Variables used for Programming */
uint8_t write_buff[128];

/* Function Prototypes */
int reprogram_ssm(uint8_t ssmID);
int initialize_reprogramming(uint8_t ssmID);
int upload_mem_to_ssm(uint32_t size, uint32_t base);
void clear_write_buff(void);
uint32_t read_signature(void);
