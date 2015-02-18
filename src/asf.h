/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved
***********************************************************************
*	FILE NAME:		asf.h
*
*	PURPOSE:
*	This file includes all API header files for the selected drivers from ASF.
*
*	FILE REFERENCES:	compiler.h, status_codes.h, gpio.h, board.h, interrupt.h,
*	pio.h, pmc.h, sleep.h, parts.h, sysclk.h, usart.h, pio_handler.h
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
*	There might be duplicate includes required by more than one driver.
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*	None.
*
*	DESCRIPTION:	Not Required.
*
*	DEVELOPMENT HISTORY:
*	11/29/2014			Header Changed.	
*
*	11/30/2014			Added an include for wdt.h (the header file for stuff involving 
*						the watchdog timer.
*
*/

#ifndef ASF_H
#define ASF_H

// From module: Common SAM compiler driver
#include <compiler.h>
#include <status_codes.h>

// From module: GPIO - General purpose Input/Output
#include <gpio.h>

// From module: Generic board support
#include <board.h>

// From module: Interrupt management - SAM3 implementation
#include <interrupt.h>

// From module: PIO - Parallel Input/Output Controller
#include <pio.h>

// From module: PMC - Power Management Controller
#include <pmc.h>
#include <sleep.h>

// From module: Part identification macros
#include <parts.h>

// From module: System Clock Control - SAM3X/A implementation
#include <sysclk.h>

// From module: USART - Univ. Syn Async Rec/Trans
#include <usart.h>

// From module: WDT - Watchdog Timer
#include <asf/sam/drivers/wdt/wdt.h>

// From module: pio_handler support enabled
#include <pio_handler.h>

// From module: RTT - Real-Time Timer
#include <asf/sam/drivers/rtt/rtt.h>

#endif // ASF_H

