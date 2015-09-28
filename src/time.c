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
*	NOTES:
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
*	DEVELOPMENT HISTORY:
*	09/27/2015			Created
*
*
*/
#include "time.h"

void delay_ms(uint32_t ms)
{
	uint32_t timeout = ms * 80000;	// Number of clock cycles needed 
	
	while(timeout--){ }
}

void delay_us(uint32_t us)
{
	uint32_t timeout = us * 80;	// Number of clock cycles needed
	
	while(timeout--){ }
}