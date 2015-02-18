/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved
***********************************************************************
*	FILE NAME:		ParTest.c
*
*	PURPOSE:
*	Simple IO routines to control the LEDs.
*
*	FILE REFERENCES:	FreeRTOS.h, task.h, partest.h, board.h, gpio.h
*
*	EXTERNAL VARIABLES:		None that I'm aware of.
*
*	EXTERNAL REFERENCES:	Many, see dependencies diagram on dropbox.
*
*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
*
*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None.
*
*	NOTES:
*	Be careful when trying to use the LED toggle and set functions defined in this file.
*	Currently the pin matching between this project and the arduino DUE is not perfect and so
*	care should be taken when trying to set particular LEDs for tests.
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*	None.
*
*	DESCRIPTION:	Not Required.
*
*	DEVELOPMENT HISTORY:
*	11/29/2014			Header Changed.
*
*/

/*-----------------------------------------------------------
 * Simple IO routines to control the LEDs.
 *-----------------------------------------------------------*/

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Demo includes. */
#include "partest.h"

/* Library includes. */
#include <board.h>
#include <gpio.h>

/* The number of LEDs available to the user on the evaluation kit. */
#define partestNUM_LEDS			( 3UL )

/* One of the LEDs is wired in the inverse to the others as it is also used as
the power LED. */
#define partstsINVERTED_LED		( 0UL )

/* The index of the pins to which the LEDs are connected.  The ordering of the
LEDs in this array is intentional and matches the order they appear on the
hardware. */
static const uint32_t ulLED[] = { LED2_GPIO, LED0_GPIO, LED1_GPIO };

/*-----------------------------------------------------------*/

/**
 * \brief Initializes the LEDs in the off state.
 */ 
void vParTestInitialise( void )
{
	unsigned long ul;

	for( ul = 0; ul < partestNUM_LEDS; ul++ )
	{
		/* Configure the LED, before ensuring it starts in the off state. */
		gpio_configure_pin( ulLED[ ul ],  ( PIO_OUTPUT_1 | PIO_DEFAULT ) );
		vParTestSetLED( ul, pdFALSE );
	}
}
/*-----------------------------------------------------------*/

/**
 * \brief Sets the LED state to high or low.
 * @param uxLED:		Number corresponding to a particular LED. If
 						this value equals the inverted LED pin, then
 						xValue, and thus the expected functionality, 
 						will be inverted.
 * @param xValue:		Boolean value - true to turn LED on,
 *									  - false to turn LED off
 */
void vParTestSetLED( unsigned portBASE_TYPE uxLED, signed portBASE_TYPE xValue )
{
	if( uxLED < partestNUM_LEDS )
	{
		if( uxLED == partstsINVERTED_LED )
		{
			xValue = !xValue;
		}

		if( xValue != pdFALSE )
		{
			/* Turn the LED on. */
			taskENTER_CRITICAL();
			{
				gpio_set_pin_low( ulLED[ uxLED ]);
			}
			taskEXIT_CRITICAL();
		}
		else
		{
			/* Turn the LED off. */
			taskENTER_CRITICAL();
			{
				gpio_set_pin_high( ulLED[ uxLED ]);
			}
			taskEXIT_CRITICAL();
		}
	}
}
/*-----------------------------------------------------------*/
/**
 * \brief Toggles the state of the LED.
 * @param uxLED:		Number corresponding to a particular LED
 */
void vParTestToggleLED( unsigned portBASE_TYPE uxLED )
{
	if( uxLED < partestNUM_LEDS )
	{
		taskENTER_CRITICAL();
		{
			gpio_toggle_pin( ulLED[ uxLED ] );
		}
		taskEXIT_CRITICAL();
	}
}

