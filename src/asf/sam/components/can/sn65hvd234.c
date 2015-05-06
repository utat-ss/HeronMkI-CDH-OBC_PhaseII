/*
Copyright (c) 2011 - 2013 Atmel Corporation. All rights reserved.
***********************************************************************
*	FILE NAME:		sn65hvd234.c
*
*	PURPOSE:
*	CAN transceiver sn65hvd234 driver.
*
*	FILE REFERENCES:	board.h, sn65hvd234.h, pio.h
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

#include "board.h"
#include "sn65hvd234.h"
#include "pio.h"

#define CAN_RS_HIGH true
#define CAN_RS_LOW  false
#define CAN_EN_HIGH true
#define CAN_EN_LOW  false

/**
 * \brief Initialize RS pin for transceiver.
 *
 * \param p_component Pointer to SN65HVD234 control structure.
 * \param pin_idx     The pin index value for transceiver RS pin.
 */
void sn65hvd234_set_rs(sn65hvd234_ctrl_t *p_component, uint32_t pin_idx)
{
	p_component->pio_rs_idx = pin_idx;
}

/**
 * \brief Initialize EN pin for transceiver.
 *
 * \param p_component Pointer to SN65HVD234 control structure.
 * \param pin_idx     The pin index value for transceiver EN pin.
 */
void sn65hvd234_set_en(sn65hvd234_ctrl_t *p_component, uint32_t pin_idx)
{
	p_component->pio_en_idx = pin_idx;
}

/**
 * \brief Enable transceiver.
 *
 * \param p_component Pointer to SN65HVD234 control structure.
 */
void sn65hvd234_enable(sn65hvd234_ctrl_t *p_component)
{
	/* Raise EN pin of SN65HVD234 to High Level (Vcc). */
	pio_set_pin_high(p_component->pio_en_idx);
	//ioport_set_pin_level(p_component->pio_en_idx, CAN_EN_HIGH);
}

/**
 * \brief Disable transceiver.
 *
 * \param p_component Pointer to SN65HVD234 control structure.
 */
void sn65hvd234_disable(sn65hvd234_ctrl_t *p_component)
{
	/* Lower EN pin of SN65HVD234 to Low Level (0.0v). */
	pio_set_pin_low(p_component->pio_en_idx);
	//ioport_set_pin_level(p_component->pio_en_idx, CAN_EN_LOW);
}

/**
 * \brief Turn the component into low power mode, listening only.
 *
 * \param p_component Pointer to SN65HVD234 control structure.
 */
void sn65hvd234_enable_low_power(sn65hvd234_ctrl_t *p_component)
{
	/* Raise RS pin of SN65HVD234 to more than 0.75v. */
	pio_set_pin_high(p_component->pio_rs_idx);
	//ioport_set_pin_level(p_component->pio_rs_idx, CAN_RS_HIGH);
}

/**
 * \brief Resume to Normal mode by exiting from low power mode.
 *
 * \param p_component Pointer to SN65HVD234 control structure.
 */
void sn65hvd234_disable_low_power(sn65hvd234_ctrl_t *p_component)
{
	/* Lower RS pin of SN65HVD234 to 0.0v~0.33v. */
	pio_set_pin_low(p_component->pio_rs_idx);
	//ioport_set_pin_level(p_component->pio_rs_idx, CAN_RS_LOW);
}

