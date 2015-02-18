/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved
***********************************************************************
*	FILE NAME:		osc.h
*
*	PURPOSE:
*	Brief Common GPIO API.
*
*	FILE REFERENCES:	parts.h, sam_ioport/sam_gpio.h, 
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
*	DESCRIPTION:	
*	This is the common API for GPIO. Additional features are available
*	in the documentation of the specific modules.
*	The following functions are available on all platforms, but there may
*	be variations in the function signature (i.e. parameters) and
*	behaviour. These functions are typically called by platform-specific
*	parts of drivers, and applications that aren't intended to be
*	portable:
*   - gpio_pin_is_low()
*   - gpio_pin_is_high()
*   - gpio_set_pin_high()
*   - gpio_set_pin_group_high()
*   - gpio_set_pin_low()
*   - gpio_set_pin_group_low()
*   - gpio_toggle_pin()
*   - gpio_toggle_pin_group()
*   - gpio_configure_pin()
*   - gpio_configure_group()
*
*	DEVELOPMENT HISTORY:
*	11/29/2014			Header Changed.
*
*/

#ifndef _GPIO_H_
#define _GPIO_H_

#include <parts.h>

#if ( SAM3S || SAM3U || SAM3N || SAM3XA || SAM4S )
# include "sam_ioport/sam_gpio.h"
#elif XMEGA
# include "xmega_ioport/xmega_gpio.h"
#else
# error Unsupported chip type
#endif

#endif  // _GPIO_H_

