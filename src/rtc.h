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
	*	FILE REFERENCES:	spi_func.h
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
	uint8_t minute;      /* minutes */
	uint8_t hour;        /* hours */
	uint8_t wday;        /* day of the week */
	uint8_t mday;        /* day of the month */
	uint8_t mon;         /* month */
	uint16_t year;       /* year */
};

/*		Control/Status Registers	*/
#define DS3234_CREG_READ		0x000E
#define DS3234_CREG_WRITE		0x008E
#define DS3234_SREG_READ		0x000F
#define DS3234_SREG_WRITE		0x008F

/*		Control Register Bits		*/
#define DS3234_A1IE     0x0001
#define DS3234_A2IE     0x0002
#define DS3234_INTCN    0x0004

/*		Status Register Bits		*/
#define DS3234_A1F      0x0001
#define DS3234_A2F      0x0002
#define DS3234_OSF      0x0080

/*			RTC API Functions		*/
void rtc_init(uint16_t creg);
void rtc_set(struct timestamp t);
void rtc_get(struct timestamp *t);

/*	Control/Status Register	Modifiers	*/
void rtc_set_creg(uint16_t val);
void rtc_set_sreg(uint16_t mask);
uint8_t rtc_get_sreg(void);

/*			RTC Alarm 2 Functions		*/
void rtc_set_a2(void);
void rtc_reset_a2(void);
void rtc_clear_a2_flag(void);
uint8_t rtc_triggered_a2(void);

/*			Support Functions				 */
void rtc_set_addr(uint16_t addr, uint16_t val);
uint8_t rtc_get_addr(uint16_t addr);