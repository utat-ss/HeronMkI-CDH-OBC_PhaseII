/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved
Copyright (C) 2012 Atmel Corporation. All rights reserved.

Edited by: Keenan Burnett
***********************************************************************
*	FILE NAME:		init.c
*
*	PURPOSE:
*	UTAT SS OBC board initialization.
*
*	FILE REFERENCES:	compiler.h, board.h, conf_board.h, gpio.h
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
*	None.
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*	None.
*
*	DESCRIPTION:	Not Required.
*
*	DEVELOPMENT HISTORY:
*	11/29/2014			Header Changed.
*	
*	11/30/2014			I added code to configure the watchdog timer and defined WATCHDOG_MR_CONFIG
*						with the appropriate values to load into the mode register.
*						
*						NOTE: WDT is an instance of the Wdt struct defined in component_wdt.h, I simply
*						had to change the value of the MDR attribute (Mode register)
*						
*						NOTE2: MDR can only be changed once until the next reset.
*
*						The code that I got is from the atmel watchdog example, check to make sure wdt.h
*						is included properly as the wdt was added to this project.
*
*	07/12/2015			I have now split up board_init() into two functions, the other being safe_board_init().
*
*						The purpose of this is that we want as little code as possible to be required in order 
*						to start up the OBC. safe_board_init() initializes thhe WDT, CAN, and interrupts.
*
*/

#include "compiler.h"
#include "board.h"
#include "conf_board.h"
#include "gpio.h"
#include "asf.h"

/************************************************************************/
/* SAFE BOARD INIT                                                      */
/************************************************************************/

void safe_board_init(void)
{	
	uint32_t wdt_mode, wdt_timer;	// Values used in initializing WDT.

	#ifndef CONF_BOARD_KEEP_WATCHDOG_AT_INIT
	/* Disable the watchdog */
	WDT->WDT_MR = WDT_MR_WDDIS;

	#endif

	#ifdef CONF_BOARD_KEEP_WATCHDOG_AT_INIT
	/* Configure WDT to trigger an interrupt (or reset). */
	wdt_mode = WDT_MR_WDFIEN |  /* Enable WDT fault interrupt. */
	WDT_MR_WDRPROC |  /* WDT fault resets processor only. */
	WDT_MR_WDIDLEHLT |
	WDT_MR_WDRSTEN;   /* WDT stops in idle state. */

	wdt_timer = 125;

	/* Initialize WDT with the given parameters. */
	wdt_init(WDT, wdt_mode, wdt_timer, wdt_timer);

	/* Configure and enable WDT interrupt. */
	NVIC_DisableIRQ(WDT_IRQn);
	NVIC_ClearPendingIRQ(WDT_IRQn);
	NVIC_SetPriority(WDT_IRQn, 0);
	NVIC_EnableIRQ(WDT_IRQn);

	// Use 	wdt_restart(WDT) to reset the watchdog timer.
	#endif

	/*Configure CAN related pins*/
	#ifdef CONF_BOARD_CAN0
	/* Configure the CAN0 TX and RX pins. */
	gpio_configure_pin(PIN_CAN0_RX_IDX, PIN_CAN0_RX_FLAGS);
	gpio_configure_pin(PIN_CAN0_TX_IDX, PIN_CAN0_TX_FLAGS);
	/* Configure the transiver0 RS & EN pins. */
	gpio_configure_pin(PIN_CAN0_TR_RS_IDX, PIN_CAN0_TR_RS_FLAGS);
	gpio_configure_pin(PIN_CAN0_TR_EN_IDX, PIN_CAN0_TR_EN_FLAGS);
	#endif

	#ifdef CONF_BOARD_CAN1
	/* Configure the CAN1 TX and RX pin. */
	gpio_configure_pin(PIN_CAN1_RX_IDX, PIN_CAN1_RX_FLAGS);
	gpio_configure_pin(PIN_CAN1_TX_IDX, PIN_CAN1_TX_FLAGS);
	/* Configure the transiver1 RS & EN pins. */
	//gpio_configure_pin(PIN_CAN1_TR_RS_IDX, PIN_CAN1_TR_RS_FLAGS);
	//gpio_configure_pin(PIN_CAN1_TR_EN_IDX, PIN_CAN1_TR_EN_FLAGS);
	#endif
	return;
}

/**
 * \brief Initialize board watchdog timer and pins.
 */
void board_init(void)
{
	/* Configure Power LED */
	gpio_configure_pin(LED3_GPIO, LED3_FLAGS);
	gpio_set_pin_high(LED3_GPIO); /* Turned on by default */

	/* Configure User LED pins */
	gpio_configure_pin(LED0_GPIO, LED0_FLAGS);
	gpio_configure_pin(LED1_GPIO, LED1_FLAGS);
	gpio_configure_pin(LED2_GPIO, LED2_FLAGS);
	
	/* Configure SSM Reset pins */
	gpio_configure_pin(EPS_RST_GPIO, EPS_RST_FLAGS);
	gpio_configure_pin(COMS_RST_GPIO, COMS_RST_FLAGS);
	gpio_configure_pin(PAY_RST_GPIO, PAY_RST_FLAGS);	
	
#ifdef CONF_BOARD_UART_CONSOLE
	/* Configure UART pins */
	gpio_configure_group(PINS_UART_PIO, PINS_UART, PINS_UART_FLAGS);
#endif

	/* Configure ADC example pins */
//#ifdef CONF_BOARD_ADC
	///* TC TIOA configuration */
	//gpio_configure_pin(PIN_TC0_TIOA0,PIN_TC0_TIOA0_FLAGS);

	/* ADC Trigger configuration */
	//gpio_configure_pin(PINS_ADC_TRIG, PINS_ADC_TRIG_FLAG);
//
	///* PWMH0 configuration */
	//gpio_configure_pin(PIN_PWMC_PWMH0_TRIG, PIN_PWMC_PWMH0_TRIG_FLAG);
//#endif

	/* Configure SPI0 pins */
#ifdef CONF_BOARD_SPI0
	gpio_configure_pin(SPI0_MISO_GPIO, SPI0_MISO_FLAGS);
	gpio_configure_pin(SPI0_MOSI_GPIO, SPI0_MOSI_FLAGS);
	gpio_configure_pin(SPI0_SPCK_GPIO, SPI0_SPCK_FLAGS);

	/**
	 * For NPCS 1, 2, and 3, different PINs can be used to access the same
	 * NPCS line.
	 * Depending on the application requirements, the default PIN may not be
	 * available.
	 * Hence a different PIN should be selected using the
	 * CONF_BOARD_SPI_NPCS_GPIO and
	 * CONF_BOARD_SPI_NPCS_FLAGS macros.
	 */

#   ifdef CONF_BOARD_SPI0_NPCS0
		gpio_configure_pin(SPI0_NPCS0_GPIO, SPI0_NPCS0_FLAGS);
#   endif

#   ifdef CONF_BOARD_SPI0_NPCS1
#       if defined(CONF_BOARD_SPI0_NPCS1_GPIO) && \
		defined(CONF_BOARD_SPI0_NPCS1_FLAGS)
			gpio_configure_pin(CONF_BOARD_SPI0_NPCS1_GPIO,
					CONF_BOARD_SPI0_NPCS1_FLAGS);
#       else
			gpio_configure_pin(SPI0_NPCS1_PA29_GPIO,
					SPI0_NPCS1_PA29_FLAGS);
#       endif
#   endif

#   ifdef CONF_BOARD_SPI0_NPCS2
#       if defined(CONF_BOARD_SPI0_NPCS2_GPIO) && \
		defined(CONF_BOARD_SPI0_NPCS2_FLAGS)
			gpio_configure_pin(CONF_BOARD_SPI0_NPCS2_GPIO,
					CONF_BOARD_SPI0_NPCS2_FLAGS);
#       else
			gpio_configure_pin(SPI0_NPCS2_PA30_GPIO,
					SPI0_NPCS2_PA30_FLAGS);
#       endif
#   endif

#   ifdef CONF_BOARD_SPI0_NPCS3
#       if defined(CONF_BOARD_SPI0_NPCS3_GPIO) && \
		defined(CONF_BOARD_SPI0_NPCS3_FLAGS)
			gpio_configure_pin(CONF_BOARD_SPI0_NPCS3_GPIO,
					CONF_BOARD_SPI0_NPCS3_FLAGS);
#       else
			gpio_configure_pin(SPI0_NPCS3_PA31_GPIO,
					SPI0_NPCS3_PA31_FLAGS);
#       endif
#   endif

#	ifdef CONF_SPI_MEM1
		gpio_configure_pin(SPI0_MEM1_HOLD, SPI0_MEM1_HOLD_FLAGS);
#	endif

#	ifdef TEMP_SENSOR
		gpio_configure_pin(TEMP_SS, TEMP_SS_FLAGS);
#	endif


#endif // #ifdef CONF_BOARD_SPI0

#ifdef CONF_BOARD_USART_RXD
	/* Configure USART RXD pin */
	gpio_configure_pin(PIN_USART0_RXD_IDX, PIN_USART0_RXD_FLAGS);
#endif

#ifdef CONF_BOARD_USART_TXD
	/* Configure USART TXD pin */
	gpio_configure_pin(PIN_USART0_TXD_IDX, PIN_USART0_TXD_FLAGS);
#endif

}

