/*
	Author: Albert Xie, Keenan Burnett
	
	***********************************************************************
	*	FILE NAME:		checksum.h
	*
	*	PURPOSE:	Houses the includes and definitions for checksum.c
	*
	*	FILE REFERENCES:	NONE
	*
	*	EXTERNAL VARIABLES:		NONE
	*
	*	EXTERNAL REFERENCES:	NONE
	*
	*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: NONE
	*
	*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:
	*	Assumes GPNVNM[2] is disabled, thus the order of flash memory is flash0 then flash1
	*
	*	NOTES: Flash0 is mapped at address 0x0008_0000
	*	
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:	NONE
	*
	*
	*	DEVELOPMENT HISTORY:		
	*	11/05/2015		K:Created
 */

/* Standard includes */
#include <stdio.h>
#include <stdint.h>

uint32_t fletcher32( uint16_t const *data, size_t words );
uint16_t fletcher16(uint8_t* data, int count);
