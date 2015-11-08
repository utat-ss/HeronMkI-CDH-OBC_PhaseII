/*
	Author: Albert Xie
	
	***********************************************************************
	*	FILE NAME:		checksum.c
	*
	*	PURPOSE:	To verify the consistency of the OBC memory once deployed.
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
	*	02/08/2015		A:Created
	*
	*	11/05/2014		K:I have added a header file for this source file so that the
	*					fletcher checksum may be used elsewhere.
	*
	*					K:Added the function fletcher16()
	*
	*	DESCRIPTION: 
	*	An optimized version of Fletcher32 checksum to be used to verify data consistency
	*	after deployment. To be run during the initial boot process in kernel mode. 
 */

/* Standard includes */
#include "checksum.h"

// Function header needed
uint32_t fletcher32( uint16_t const *data, size_t words )
{
	/* sum1 and sum2 should never be 0 */
	uint32_t sum1 = 0xffff;
	uint32_t sum2 = 0xffff;
	
	while (words)
	{
		/* 359 is the largest n such that ( n(n+1) / 2 ) will not cause an overflow in sum2 */
		unsigned len = words > 359 ? 359 : words;
		words -= len;

		while(--len)
		{
			sum2 += sum1 += *(data++);
		}
		
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	}
	/* Second reduction step to reduce sums to 16 bits to yield a final uint32_t */
	sum1 = (sum1 & 0xffff) + (sum1 >> 16);
	sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	
	return sum2 << 16 | sum1;
}

// Simpler version of the checkum above, returns a 16-bit checksum.
uint16_t fletcher16(uint8_t* data, int count)
{
	uint16_t sum1 = 0;
	uint16_t sum2 = 0;
	int i;
	
	for(i = 0; i < count; i++)
	{
		sum1 = (sum1 + data[i]) % 255;
		sum2 = (sum2 + sum1) % 255;
	}
	
	return (sum2 << 8) | sum1;
}
