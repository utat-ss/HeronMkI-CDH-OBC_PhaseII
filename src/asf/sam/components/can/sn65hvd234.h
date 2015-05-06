/*
Copyright (c) 2011 - 2013 Atmel Corporation. All rights reserved.
***********************************************************************
*	FILE NAME:		sn65hvd234.h
*
*	PURPOSE:
*	CAN transceiver sn65hvd234 driver function prototypes.
*
*	FILE REFERENCES:	board.h
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
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*	None.
*
*	DESCRIPTION:
*
*	This driver provides access to the main features of the SN65HVD234
*	transceiver.
*
*	Control the RS and EN pin level for SN65HVD234.
*
*	DEVELOPMENT HISTORY:
*	11/29/2014			Header Changed.
*
*/

#ifndef CAN_SN65HVD234_H
#define CAN_SN65HVD234_H

#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	/** PIO dedicated to RS pin index. */
	uint32_t pio_rs_idx;
	/** PIO dedicated to EN pin index. */
	uint32_t pio_en_idx;
} sn65hvd234_ctrl_t;

void sn65hvd234_set_rs(sn65hvd234_ctrl_t *p_component, uint32_t pin_idx);
void sn65hvd234_set_en(sn65hvd234_ctrl_t *p_component, uint32_t pin_idx);
void sn65hvd234_enable(sn65hvd234_ctrl_t *p_component);
void sn65hvd234_disable(sn65hvd234_ctrl_t *p_component);
void sn65hvd234_enable_low_power(sn65hvd234_ctrl_t *p_component);
void sn65hvd234_disable_low_power(sn65hvd234_ctrl_t *p_component);

#ifdef __cplusplus
}
#endif
#endif /* CAN_SN65HVD234_H */
