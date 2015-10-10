/*
	Author: Keenan Burnett

	***********************************************************************
*	FILE NAME:		time.c
*
*	PURPOSE:		This file is used for delay functions or functions related to timers.
*
*	FILE REFERENCES:		time.h
*
*	EXTERNAL VARIABLES:
*
*	EXTERNAL REFERENCES:	Same a File References.
*
*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
*
*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
*
*	NOTES:	These functions are NOT implemented with internal timers,
*			your process' execution will stop during the use of these functions.
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
*	DEVELOPMENT HISTORY:
*	09/27/2015			Created
*
*
*/
#include "time.h"

void delay_s(uint32_t s)
{
	uint32_t timeout = s * 8400000;	// NUmber of clock cycles needed.
	while(timeout--){ }
	return;
}

void delay_ms(uint32_t ms)
{
	uint32_t timeout = ms * 84000;	// Number of clock cycles needed 
	while(timeout--){ }
	return;
}

void delay_us(uint32_t us)
{
	uint32_t timeout = us * 84;	// Number of clock cycles needed
	while(timeout--){ }
	return;
}

