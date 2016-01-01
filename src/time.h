/*
	Author: Keenan Burnett

	***********************************************************************
*	FILE NAME:		time.h
*
*	PURPOSE:		This file houses the includes, definitions, and prototypes for time.c
*
*	FILE REFERENCES:		None
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
#ifndef TIMEH
#define TIMEH

#include <stdint.h>

void delay_s(uint32_t s);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

#endif