/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved
***********************************************************************
*	FILE NAME:		sam_gpio.h
*
*	PURPOSE:
*	Brief GPIO service for SAM.
*
*	FILE REFERENCES:	compiler.h, pio.h
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
*/

#ifndef SAM_GPIO_H_INCLUDED
#define SAM_GPIO_H_INCLUDED

#include "compiler.h"
#include "pio.h"

#define gpio_pin_is_low(io_id) \
	(pio_get_pin_value(io_id) ? 0 : 1)

#define gpio_pin_is_high(io_id) \
	(pio_get_pin_value(io_id) ? 1 : 0)

#define gpio_set_pin_high(io_id) \
	pio_set_pin_high(io_id)

#define gpio_set_pin_low(io_id) \
	pio_set_pin_low(io_id)

#define gpio_toggle_pin(io_id) \
	pio_toggle_pin(io_id)

#define gpio_configure_pin(io_id,io_flags) \
	pio_configure_pin(io_id,io_flags)

#define gpio_configure_group(port_id,port_mask,io_flags) \
	pio_configure_pin_group(port_id,port_mask,io_flags)

#define gpio_set_pin_group_high(port_id,mask) \
	pio_set_pin_group_high(port_id,mask)

#define gpio_set_pin_group_low(port_id,mask) \
	pio_set_pin_group_low(port_id,mask)

#define gpio_toggle_pin_group(port_id,mask) \
	pio_toggle_pin_group(port_id,mask)

#endif /* SAM_GPIO_H_INCLUDED */

