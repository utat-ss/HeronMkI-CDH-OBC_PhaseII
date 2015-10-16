/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		spimemtest.c
	*
	*	PURPOSE:		
	*	This file is to be used to create the housekeeping task needed to monitor
	*	housekeeping information on the satellite.
	*
	*	FILE REFERENCES:	stdio.h, FreeRTOS.h, task.h, partest.h, asf.h, can_func.h
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
	*	New tasks should be written to use as much of CMSIS as possible. The ASF and 
	*	FreeRTOS API libraries should also be used whenever possible to make the program
	*	more portable.

	*	DEVELOPMENT HISTORY:		
	*	10/07/2015			Created.
	*
	*	DESCRIPTION:	
	*
	*	This program is meant to be used to test the functionality of the SPI Memory in the future
	*	to make sure that software and hardware are both working properly.
	*
	*	Please insert this program into the main repository under ../src/
	*
	*	I have instructions for places to set breakpoints, this is the easiest way to see whether
	*	or not the program has succeeded. I also blink LEDs because it makes me happy.
	*	
 */

/* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Atmel library includes. */
#include "asf.h"

/* Common demo includes. */
#include "partest.h"

/* Spi Memory includes 	 */
#include "spimem.h"

#include "gpio.h"

/* Priorities at which the tasks are created. */
#define SMT_PRIORITY		( tskIDLE_PRIORITY + 2 )		// Lower the # means lower the priority

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define SMT_PARAMETER			( 0xABCD )

/*-----------------------------------------------------------*/
uint8_t test_page1[256], test_page2[256];
uint8_t spi_test_buffer[4096];
uint32_t rand_addr = 0x00000;		// Random address to test 256 Byte writing.
/*
 * Functions Prototypes.
 */
static void prvspimemtestTask( void *pvParameters );
void spimemtest(void);
static void test1_SimpleWriteAndRead(uint8_t spi_chip);
static void test2_SectorEraseAndRead(uint8_t spi_chip);
static void test3_SectorWriteAndRead(uint8_t spi_chip);
static void test4_SectorEraseAndRead(uint8_t spi_chip);
static void test5_BoundaryWriteAndRead(uint8_t spi_chip);
static void test6_WriteDirtyPageAndRead(uint8_t spi_chip);
static void test7_WriteDirtyPageAndReadAtBoundary(uint8_t spi_chip);

/************************************************************************/
/* SPIMEMTEST 															*/
/* @Purpose: This function is simply used to create the spimemtesttask  */
/* in main.c															*/
/************************************************************************/
void spimemtest( void )
{
	/* Start the two tasks as described in the comments at the top of this
	file. */
	xTaskCreate( prvspimemtestTask,					/* The function that implements the task. */
				"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
				configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
				( void * ) SMT_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
				SMT_PRIORITY, 			/* The priority assigned to the task. */
				NULL );								/* The task handle is not required, so NULL is passed. */
					
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	return;
}

/************************************************************************/
/*				PRVSPIMEMTESTTASK		                                */
/*	The sole purpose of this task is to test the functionality of our	*/
/* code for the spi memory chips which is contained in spimem.c/.h		*/
/************************************************************************/
static void prvspimemtestTask(void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == SMT_PARAMETER );
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 250;	// Number entered here corresponds to the number of ticks we should wait.
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
	uint32_t i;

	for(i = 0; i < 256; i++)
	{
		test_page1[i] = (uint8_t)i;
	}

	for(i = 0; i < 256; i++)
	{
		test_page2[i] = 0;
	}

	/* @non-terminating@ */	
	for( ;; )
	{
		test1_SimpleWriteAndRead(2);

		test2_SectorEraseAndRead(2);

		test3_SectorWriteAndRead(2);

		test4_SectorEraseAndRead(2);

		test5_BoundaryWriteAndRead(2);

		test6_WriteDirtyPageAndRead(2);

		test7_WriteDirtyPageAndReadAtBoundary(2);

		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
	}
}
/*-----------------------------------------------------------*/

static void test1_SimpleWriteAndRead(uint8_t spi_chip)
{
	uint32_t rand_addr = 0x00000;
	int x, y, z;
	uint32_t i = 0;
	uint8_t test;

	x = spimem_write(spi_chip, rand_addr, test_page1, 256);		// Write 256 Bytes to a single page in chip 2.
	y = spimem_read(spi_chip, rand_addr, test_page2, 256);			// Read it back.

	for(i = 0; i < 256; i++)
	{
		test = test_page2[i];
		if(test_page1[i] != test_page2[i])
			z = 1;											// z = 1 is the arrays are not equal.
	}

	// SET BREAKPOINT AT PIO_TOGGLE_PIN.
	if((x == -1) || (y == -1) || (z == 1))
		pio_toggle_pin(LED3_GPIO);							// Test 1 Failed.

	return;
}

static void test2_SectorEraseAndRead(uint8_t spi_chip)
{
	uint32_t rand_addr = 0x00000;
	uint32_t temp;
	int x, y, z;
	uint32_t i;
	
	for(i = 0; i < 4096; i++)			// Fill up the test buffer with something we can verify.
	{
		spi_test_buffer[i] = 0xFF;
	}

	x = erase_sector_on_chip(spi_chip, 0);
	y = load_sector_into_spibuffer(spi_chip, 0);

	for(i = 0; i < 4096; i++)
	{
		if(spi_mem_buff[i] != spi_test_buffer[i])
			z = 1;											// z = 1 is the arrays are not equal.
	}

	// SET BREAKPOINT AT PIO_TOGGLE_PIN.
	if((x =! 1) || (y =! 1) || (z == 1))
		pio_toggle_pin(LED3_GPIO);							// Test 2 Failed.

	return;
}

static void test3_SectorWriteAndRead(uint8_t spi_chip)
{
	uint32_t rand_addr = 0x00000;
	uint32_t temp;
	int x, y, z;
	uint32_t i;

	for (i = 0; i < 128; i++)
	{
		spi_bit_map[i] = 0;				// Initialize the bitmap
	}
	
	for(i = 0; i < 4096; i++)			// Fill up the test buffer with something we can verify.
	{
		spi_mem_buff[i] = (uint8_t)(i % 256);
	}
	for(i = 0; i < 4096; i++)			// Fill up the test buffer with something we can verify.
	{
		spi_test_buffer[i] = (uint8_t)(i % 256);
	}
	
	spi_mem_buff_sect_num = 0;
	temp = write_sector_back_to_spimem(spi_chip);						// write spimem_buffer contents to chip.
	x = (int)temp;
	y = load_sector_into_spibuffer(spi_chip, 0);

	for(i = 0; i < 4096; i++)
	{
		if(spi_mem_buff[i] != spi_test_buffer[i])
			z = 1;											// z = 1 is the arrays are not equal.
	}

	// SET BREAKPOINT AT PIO_TOGGLE_PIN.
	if((x == -1) || (y == -1) || (z == 1))
		pio_toggle_pin(LED3_GPIO);							// Test 3 Failed.

	return;
}

static void test4_SectorEraseAndRead(uint8_t spi_chip)
{
	uint32_t rand_addr = 0x00000;
	uint32_t temp;
	int x, y, z;
	uint32_t i;
	
	for(i = 0; i < 4096; i++)			// Fill up the test buffer with something we can verify.
	{
		spi_test_buffer[i] = 0xFF;
	}

	x = erase_sector_on_chip(spi_chip, 0);
	y = load_sector_into_spibuffer(spi_chip, 0);

	for(i = 0; i < 4096; i++)
	{
		if(spi_mem_buff[i] != spi_test_buffer[i])
			z = 1;											// z = 1 is the arrays are not equal.
	}
	
	for (i = 0; i < 128; i++)
	{
		spi_bit_map[i] = 0;				// Initialize the bitmap
	}

	// SET BREAKPOINT AT PIO_TOGGLE_PIN.
	if((x == -1) || (y == -1) || (z == 1))
		pio_toggle_pin(LED3_GPIO);							// Test 2 Failed.

	return;
}

static void test5_BoundaryWriteAndRead(uint8_t spi_chip)
{
	uint32_t rand_addr = 0x0000F;
	int x, y, z;
	uint32_t i;

	x = spimem_write(spi_chip, rand_addr, test_page1, 256);		// Write 256 Bytes to 2 pages in chip 2.
	y = spimem_read(spi_chip, rand_addr, test_page2, 256);			// Read it back.

	for(i = 0; i < 256; i++)
	{
		if(test_page1[i] != test_page2[i])
			z = 1;											// z = 1 is the arrays are not equal.
	}

	// SET BREAKPOINT AT PIO_TOGGLE_PIN.
	if((x == -1) || (y == -1) || (z == 1))
		pio_toggle_pin(LED3_GPIO);							// Test 5 Failed.

	return;
}

static void test6_WriteDirtyPageAndRead(uint8_t spi_chip)
{
	uint32_t rand_addr = 0x00000;
	int x, y, z;
	uint32_t i;

	x = spimem_write(spi_chip, rand_addr, test_page1, 256);		// Write 256 Bytes to 2 pages in chip 2.
	y = spimem_read(spi_chip, rand_addr, test_page2, 256);			// Read it back.

	for(i = 0; i < 256; i++)
	{
		if(test_page1[i] != test_page2[i])
			z = 1;											// z = 1 is the arrays are not equal.
	}

	// SET BREAKPOINT AT PIO_TOGGLE_PIN.
	if((x == -1) || (y == -1) || (z == 1))
		pio_toggle_pin(LED3_GPIO);							// Test 6 Failed.

	return;	
}

static void test7_WriteDirtyPageAndReadAtBoundary(uint8_t spi_chip)
{
	uint32_t rand_addr = 0x00000;
	int x, y, z;
	uint32_t i;

	x = spimem_write(spi_chip, rand_addr, test_page1, 256);		// Write 256 Bytes to 2 pages in chip 2.
	y = spimem_read(spi_chip, rand_addr, test_page2, 256);			// Read it back.

	for(i = 0; i < 256; i++)
	{
		if(test_page1[i] != test_page2[i])
			z = 1;											// z = 1 is the arrays are not equal.
	}

	// SET BREAKPOINT AT PIO_TOGGLE_PIN.
	if((x == -1) || (y == -1) || (z == 1))
		pio_toggle_pin(LED3_GPIO);							// Test 7 Failed.

	return;	
}

