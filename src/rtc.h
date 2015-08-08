/*
   	Original Author:          Petre Rodan <petre.rodan@simplex.ro>
   	Available from:  https://github.com/rodan/ds3234
    License:         GNU GPLv3
		
	Modified by: Omar Abdeldayem
	***********************************************************************
	*	FILE NAME:		rtc.h
	*
	*	PURPOSE:	This file contains includes and definitions for rtc.c		
	*
	*	FILE REFERENCES:	
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
	*	07/018/2015		Created.
	*
	*	DESCRIPTION:	
	*					Includes and definitions for rtc.c	
	*
	*	
 */

#include "spi_func.h"

/*		Timestamp Struct			*/
struct timestamp
{
	uint8_t sec;         /* seconds */
	uint8_t min;         /* minutes */
	uint8_t hour;        /* hours */
	uint8_t mday;        /* day of the month */
	uint8_t mon;         /* month */
	int year;            /* year */
	uint8_t wday;        /* day of the week */
	uint8_t yday;        /* day in the year */
	uint8_t isdst;       /* daylight saving time */
	uint8_t year_s;      /* year in short notation*/
};

/*		Control/Status Registers	*/
#define DS3234_CREG_READ		0x0E
#define DS3234_CREG_WRITE		0x8E
#define DS3234_SREG_READ		0x0F
#define DS3234_SREG_WRITE		0x8F

/*		Control Register Bits		*/
#define DS3234_A1IE     0x1
#define DS3234_A2IE     0x2
#define DS3234_INTCN    0x4

/*		Status Register Bits		*/
#define DS3234_A1F      0x1
#define DS3234_A2F      0x2
#define DS3234_OSF      0x80

/*			RTC API Functions		*/
void rtc_init(const uint8_t creg);
void rtc_set(struct timestamp t);
void rtc_get(struct timestamp *t);

/*	Control/Status Register	Modifiers	*/
void rtc_set_creg(const uint8_t val);
void rtc_set_sreg(const uint8_t mask);
uint8_t rtc_get_sreg(void);

/*					Support Functions					 */
void rtc_set_addr(const uint8_t addr, const uint8_t val);
uint8_t rtc_get_addr(const uint8_t addr);