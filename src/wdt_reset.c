/*
 * FILE NAME: wdt_reset.c
 *
 * Created: 10/10/2015 6:20:44 PM
 *  Author: Ali
 */ 


/* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Standard demo includes - just needed for the LED (ParTest) initialization
function. */
#include "partest.h"

/* Atmel library includes. */
#include <asf.h>
//What other includes do I need?

/* Priorities at which the task is created. */
#define WDT_Reset_PRIORITY        (tskIDLE_PRIORITY + 1)

/* Time that task is delayed for after running. UNITS? */
#define WDT_Reset_Delay				   100 


/* Values passed to the two tasks just to check the task parameter
functionality. */
#define WDT_PARAMETER			( 0xABCD ) // WHAT IS THIS?

static void wdtResetTask( void *pvParameter); // I don't know what this does
void wdt_reset(void);

void wdt_reset(void);
{
	/*Start the watchdog timer reset task as described in comments */
	
	xTaskCreate( wdtResetTask,			/* The function that implements the task. */
	"ON", 								/* The text name assigned to the task - for debug only as it is not used by the kernel. */
	configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
	( void * ) WDT_PARAMETER, 			/* The parameter passed to the task - just to check the functionality. */
	WDT_Reset_PRIORITY, 			    /* The priority assigned to the task. */
	NULL );								/* The task handle is not required, so NULL is passed. */
	
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	return;
	
	//What happens if it does return?
	
}


/************************************************************************/
/*				WDT RESET TASK		                                */
/*	The purpose of this task is to reset the watchdog timer every  	*/
/*	time interval (defined by WDT_Reset_Delay						*/
/************************************************************************/
