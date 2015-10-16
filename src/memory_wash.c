/*
    Author: Keenan Burnett

	***********************************************************************
	*	FILE NAME:		memory_wash.c
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
	*	10/15/2015		Created.
	*
	*	DESCRIPTION:	
	*
	*	This task shall periodically "wash" the external SPI Memory by checking for bit flips and
	*	subsequently correcting them.
	*
	*	In order to do this, we intend to have 3 external SPI Memory chips and use a "voting"
	*	method in order to correct bit flips on the devices.
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

/* Common includes. */
#include "partest.h"

#include "spimem.h"

/* Priorities at which the tasks are created. */
#define MemoryWash_PRIORITY		( tskIDLE_PRIORITY + 4 )		// Lower the # means lower the priority

/* Values passed to the two tasks just to check the task parameter
functionality. */
#define MW_PARAMETER			( 0xABCD )

/*-----------------------------------------------------------*/

static uint8_t page_buff1[256], page_buff2[256], page_buff3[256]; 
/*
 * Functions Prototypes.
 */
static void prvMemoryWashTask( void *pvParameters );
void menory_wash(void);

/************************************************************************/
/* MEMORY_WASH (Function)												*/
/* @Purpose: This function is used to create the memory washing task.	*/
/************************************************************************/
void memory_wash( void )
{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvMemoryWashTask,					/* The function that implements the task. */
					"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
					( void * ) MW_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
					MemoryWash_PRIORITY, 			/* The priority assigned to the task. */
					NULL );								/* The task handle is not required, so NULL is passed. */
					
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	return;
}

/************************************************************************/
/*				MEMORY WASH (TASK)		                                */
/*	This task will periodically take control of the SPI Memory Chips and*/
/* verify that there are no bit flips within them by way of a voting    */
/* algorithm between 3 separate chips storing redundant data.			*/
/************************************************************************/
static void prvMemoryWashTask(void *pvParameters )
{
	configASSERT( ( ( unsigned long ) pvParameters ) == MW_PARAMETER );
	TickType_t	xLastWakeTime;
	const TickType_t xTimeToWait = 5400000;	// Number entered here corresponds to the number of ticks we should wait (90 minutes)
	/* As SysTick will be approx. 1kHz, Num = 1000 * 60 * 60 = 1 hour.*/
	
	int x, y, z;
	uint8_t spi_chip, write_required, sum, correct_val;	// write_required can be either 1, 2, 3, or 4 to indicate which chip needed a write.
	uint32_t page, addr, byte;
		
	/* @non-terminating@ */	
	for( ;; )
	{
		for(page = 0; page < 4096; page++)	// Loop through each page in memory.
		{
			addr = page << 8;
			
			for(spi_chip = 1; spi_chip < 4; spi_chip++)
			{
				x = spimem_read(spi_chip, addr, page_buff1, 256);
				y = spimem_read(spi_chip, addr, page_buff2, 256);
				z = spimem_read(spi_chip, addr, page_buff3, 256);	
					
				if((x < 0) || (y < 0) || (z < 0))
					x = x;									// FAILURE_RECOVERY.
			}
			
			for(byte = 0; byte < 256; byte++)
			{
				sum = page_buff1[byte] + page_buff2[byte] + page_buff3[byte];
				
				if(sum < 3)
				{
					if(sum == 2)
						correct_val = 1;
					if(sum < 2)
						correct_val = 0;
				}
				else
					correct_val = sum / 3;
					
				if(page_buff1[byte] != correct_val)
				{
					x = spimem_write(1, addr, &correct_val, 1);		// Correct the byte on that chip.
				}
				if(page_buff2[byte] != correct_val)
				{
					y = spimem_write(2, addr, &correct_val, 1);
				}
				if(page_buff3[byte] != correct_val)
				{
					z = spimem_write(3, addr, &correct_val, 1);
				}
				
				if((x < 0) || (y < 0) || (z < 0))
					x = x;									// FAILURE_RECOVERY.
			}
			
		}
		xLastWakeTime = xTaskGetTickCount();
		vTaskDelayUntil(&xLastWakeTime, xTimeToWait);
	}
}
/*-----------------------------------------------------------*/

