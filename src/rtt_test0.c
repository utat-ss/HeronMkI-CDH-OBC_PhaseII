/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved
***********************************************************************
*	FILE NAME:		rtt_test0.c
*
*	PURPOSE:
*	This program shall be used to check and test the functionality of the Real-Time Timer
*
*	FILE REFERENCES:	asf.h, conf_board.h, conf_clock.h, stdio_serial.h
*
*	EXTERNAL VARIABLES:		g_uc_state, g_ul_new_alarm, g_uc_alarmed are volatiles.
*
*	EXTERNAL REFERENCES:	Same as file references.
*
*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES:
*	
*	For some reason the RTT will not trigger an alarm interrupt if the tick
*	interrupt is disabled, this should be fixed as we don't need the tick interrupt.
*
*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
*
*	NOTES:
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:

*	DEVELOPMENT HISTORY:
*	01/01/2015		Created.
*
*	DESCRIPTION:
*	The Real-Time Timer in this test is setup for 1 second ticks (as per normal)
*	and has an alarm set for every 5 + 1 seconds = ALMV + 1, this is defined by
*	g_ul_new_alarm. Both the ticks and alarms trigger an interrupt of programmable
*	priority (currently 14, which is 1 better than SysTick.) The interrupt handler is
*	RTT_Handler().
*
*/

#include "asf.h"
#include "conf_board.h"
#include "conf_clock.h"

/** Current device state. */
volatile uint8_t g_uc_state;

/** New alarm time being currently entered. */
volatile uint32_t g_ul_new_alarm;

/** Indicate that an alarm has occurred but has not been cleared. */
volatile uint8_t g_uc_alarmed;

/************************************************************************/
/*					RTT CONFIGURATION FUNCTION				            */
/*		Configure the RTT to generate a one second tick, which triggers */
/*		the RTTINC interrupt.											*/
/************************************************************************/
/**
 * \brief Configure the RTT to generate a one second tick, which triggers
 * the RTTINC interrupt.
 */
static void configure_rtt(void)
{
	uint32_t ul_previous_time;

	/* Configure RTT for a 1 second tick interrupt */
	rtt_init(RTT, 32768);

	ul_previous_time = rtt_read_timer_value(RTT);
	while (ul_previous_time == rtt_read_timer_value(RTT));

	/* Enable RTT interrupt */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 0);
	NVIC_EnableIRQ(RTT_IRQn);
	rtt_enable_interrupt(RTT, RTT_MR_RTTINCIEN);
}

/************************************************************************/
/*					REAL-TIME TIMER INTERRUPT HANDLER                   */
/************************************************************************/
/**
 * \brief Real-time timer interrupt handler.
 */
void RTT_Handler(void)
{
	uint32_t ul_status;

	/* Get RTT status */
	ul_status = rtt_get_status(RTT); // Reading the status register clears the interrupt requests.

	/* Time has changed*/
	pio_toggle_pin(LED0_GPIO);
		
	/* Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		g_uc_alarmed = 1;
		pio_toggle_pin(LED1_GPIO);
		rtt_init(RTT, 32768);
		rtt_enable_interrupt(RTT, RTT_MR_RTTINCIEN);
		rtt_write_alarm_time(RTT, g_ul_new_alarm);
	}
}

/**
 * \brief Test the functionality of the RTT handler
 * @return:			Default zero		
 */
int rtt_test0(void)
{
	/* Configure RTT */
	configure_rtt();
	
	/* Configure Alarm */
	g_ul_new_alarm = 5;
	g_uc_alarmed = 0;
	rtt_write_alarm_time(RTT, g_ul_new_alarm);
	/* The RTT tick interrupt will now go off every 1 second and the 
	*  RTT Alarm interrupt will go off however many seconds are defined by
	*  g_ul_new_alarm.
	*/

	/* @non-terminating@ */
	while(1) { } 
		
	return 0;
}

