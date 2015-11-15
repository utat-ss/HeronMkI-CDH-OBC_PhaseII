/*
	Author: Albert Xie, Keenan Burnett
	
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
	*	11/12/2015		K:I added functions fletcher64() and fletcher64_on_spimem()
	*
	*	DESCRIPTION: 
	*	An optimized version of Fletcher32 checksum to be used to verify data consistency
	*	after deployment. To be run during the initial boot process in kernel mode. 
 */

/* Standard includes */
#include "checksum.h"

static void clear_check_array(void);

/************************************************************************/
/* FLETCHER64				                                            */
/* @Purpose: This function runs Fletcher's checksum algorithm on memory	*/
/* @param: *data: pointer to the point in memory that you would to start*/
/* hashing.																*/
/* @param: count: how many BYTES in memory, you would like to hash		*/
/* @return: the 64-bit checksum value.									*/
/************************************************************************/
uint64_t fletcher64(uint32_t* data, int count)
{
	uint64_t sum1 = 0, sum2 = 0;
	int count_ints = count / 4;
	int i;
	for(i = 0; i < count_ints; i++)
	{
		sum1 = (sum1 + data[i]) % (uint64_t)0x100000000;
		sum2 = (sum2 + sum1) % (uint64_t)0x100000000;
	}
	return (sum2 << 32) | sum1;
}

/************************************************************************/
/* FLETCHER64				                                            */
/* @Purpose: This function runs Fletcher's checksum algorithm on spimem	*/
/* @param: *data: pointer to the point in memory that you would to start*/
/* hashing.																*/
/* @param: count: how many BYTES in memory, you would like to hash		*/
/* @param: *status: 0xFF = failure, 0x01 = success.						*/	
/* @return: the 64-bit checksum value.									*/
/************************************************************************/
uint64_t fletcher64_on_spimem(uint32_t address, int count, uint8_t* status)
{
	uint8_t i, j;
	int num = 0;
	uint64_t sum1 = 0, sum2 = 0;
	uint8_t num_pages = (count / 256);
	if(count % 256)
		num_pages++;
	clear_check_array();
	for(i = 0; i < num_pages; i++)
	{
		if(spimem_read((address + i * 256), check_arr, (count - i * 256)) < 0)
		{
			*status = 0xFF;
			return 0;
		}
		for(j = 0; j < 256; j+=4)
		{
			num = (int)check_arr[j];
			num += ((int)check_arr[j + 1]) << 8;
			num += ((int)check_arr[j + 2]) << 16;
			num += ((int)check_arr[j + 3]) << 24;
			
			sum1 = (sum1 + num) % (uint64_t)0x100000000;
			sum2 = (sum2 + sum1) % (uint64_t)0x100000000;
		}
	}
	*status = 1;
	return (sum2 << 32) | sum1;
}

/************************************************************************/
/* FLETCHERd32				                                            */
/* @Purpose: This function runs Fletcher's checksum algorithm on memory	*/
/* @param: *data: pointer to the point in memory that you would to start*/
/* hashing.																*/
/* @param: words: how many WORDS in memory, you would like to hash		*/
/* @return: the 32-bit checksum value.									*/
/************************************************************************/
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

/************************************************************************/
/* FLETCHER16				                                            */
/* @Purpose: This function runs Fletcher's checksum algorithm on spimem	*/
/* @param: *data: pointer to the point in memory that you would to start*/
/* hashing.																*/
/* @param: count: how many BYTES in memory, you would like to hash		*/
/* @return: the 16-bit checksum value.									*/
/************************************************************************/
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

/************************************************************************/
/* CLEAR_CHECK_ARRAY		                                            */
/* @Purpose: This function clears the check_arr[] array.				*/
/************************************************************************/
static void clear_check_array(void)
{
	uint16_t i;
	for(i = 0; i < 256; i++)
	{
		check_arr[i] = 0;
	}
	return;
}
