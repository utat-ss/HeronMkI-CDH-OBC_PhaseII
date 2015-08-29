/*
	Original Author:          Petre Rodan <petre.rodan@simplex.ro>
	Available from:  https://github.com/rodan/ds3234
	License:         GNU GPLv3
	
    Modified by: Omar Abdeldayem
	***********************************************************************
	*	FILE NAME:		rtc.c
	*
	*	PURPOSE:		DS3234 Real Time Clock functionality through SPI, using the ATSAM3X8E as Master. 
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
	*	08/08/2015		Setting and reading time from the RTC is now functional.
	*	
	*   08/29/2015      Removed the added century bit from month (unnecessary) and simplified the
	*					timestamp structure to contain only the essential information.
	*
	*	DESCRIPTION:	
	*			
	*					Provides the functionality to use the DS3234 as an external RTC using SPI. 
	*					
    *					Retrieving and setting the time/date are currently supported.
	*					
	*					Alarms (1 & 2), SRAM, square wave generator and watchdog timer functionality of the chip
	*					are disabled.
 */
#include "rtc.h"

/** 
 * \brief Decimal to binary coded decimal conversion
 */
static uint8_t dectobcd(uint8_t val)
{
	return ((val / 10 * 16) + (val % 10));
}

/** 
 * \brief Binary coded decimal to decimal conversion
 */
static uint8_t bcdtodec(uint8_t val)
{
	return ((val / 16 * 10) + (val % 16));
}

/**
 * \brief Initialize the RTC by configuring the control register and 
 *  setting the time-date to 00:00:00 01/01/0000
 *
 * \param ctrl_reg_val The byte to set the control register to
 */
void rtc_init(uint16_t ctrl_reg_val)
{			
    rtc_set_creg(ctrl_reg_val);
	
	struct timestamp initial_time;

	initial_time.sec = 0x00;
	initial_time.minute = 0x00;
	initial_time.hour = 0x00;
	initial_time.mday = 0x01;
	initial_time.wday = 0x01;
	initial_time.mon = 0x01;
	initial_time.year = 0x00;
	
	rtc_set(initial_time);
}

/**
 * \brief Set the time and date of the RTC to a specified value.
 *
 * \param t The time struct containing the new time/date to update to.
 */
void rtc_set(struct timestamp t)
{
	uint8_t time_date[7] = { t.sec, t.minute, t.hour, t.wday, t.mday, t.mon, t.year };
    uint8_t i, century;
	uint16_t message, buffer_0, buffer_1, buffer_2;

    for (i = 0; i < 7; i++) 
	{
		// Convert data and prepare message to send
		buffer_0 = i + 0x80;
		buffer_2 = dectobcd(time_date[i]);
		
		message = (buffer_0 << 8) | buffer_2;
		spi_master_transfer(&message, 1);
    }
}

/** 
 * \brief Retrieves the current time and date from the RTC.
 *
 * \param t Pointer to an empty timestamp struct that will contain
 *  the values retrieved from the RTC. 
 */
void rtc_get(struct timestamp *t)
{	
    uint8_t time_date[7];        // second, minute, hour, day of week, day of month, month, year
    uint8_t i, ret_val, century = 0;
    uint16_t year_full, message;
	
    for (i = 0; i < 7; i++) 
	{
		// Message contains the register address then an empty byte to flush it out
		message = i + 0x00;
		message = message << 8;

		spi_master_transfer(&message, 1);
		
		// Get the value flushed out of the register
		ret_val = (uint8_t) message;
		time_date[i] = bcdtodec(ret_val);	
    }

	// Store values into timestamp provided
    t->sec = time_date[0];
    t->minute = time_date[1];
    t->hour = time_date[2];
	t->wday = time_date[3];
    t->mday = time_date[4];
    t->mon = time_date[5];
    t->year = time_date[6]; 
}

/** 
 * \brief Sets an RTC register to a specified value.
 *
 * \param addr RTC register address
 * \param val  New register value
 */
void rtc_set_addr(uint16_t addr, uint16_t val)
{
	uint16_t message = (addr << 8) | val;
	spi_master_transfer(&message, 1);
}

/** 
 * \brief Gets the value from a register.
 *
 * \param addr RTC register address
 *
 * \return val Value stored in specified register
 */
uint8_t rtc_get_addr(uint16_t addr)
{
	uint8_t val;
	uint16_t message = (uint16_t) addr << 8;
	spi_master_transfer(&message, 1);
	
	val = (uint8_t) (message | 0x00FF);	
	return val;
}


/** 
 * \brief Sets the control register of the RTC to a specified value.
 *
 * \param val  Value to write to the control register, each bit corresponding to the following:
 * bit7 EOSC   Enable Oscillator (1 if oscillator must be stopped when on battery)
 * bit6 BBSQW  Battery Backed Square Wave
 * bit5 CONV   Convert temperature (1 forces a conversion NOW)
 * bit4 RS2    Rate select - frequency of square wave output
 * bit3 RS1    Rate select
 * bit2 INTCN  Interrupt control (1 for use of the alarms and to disable square wave)
 * bit1 A2IE   Alarm1 interrupt enable (1 to enable)
 * bit0 A1IE   Alarm0 interrupt enable (1 to enable)
 */
void rtc_set_creg(uint16_t val)
{
    rtc_set_addr(DS3234_CREG_WRITE, val);
}

/**
 * \brief Sets the status register of the RTC to a specific value.
 *
 * \param val    Value to write to the status register, each bit corresponding to the following:
 * bit7 OSF      Oscillator Stop Flag (if 1 then oscillator has stopped and date might be innacurate)
 * bit6 BB32kHz  Battery Backed 32kHz output (1 if square wave is needed when powered by battery)
 * bit5 CRATE1   Conversion rate 1  temperature compensation rate
 * bit4 CRATE0   Conversion rate 0  temperature compensation rate
 * bit3 EN32kHz  Enable 32kHz output (1 if needed)
 * bit2 BSY      Busy with TCXO functions
 * bit1 A2F      Alarm 1 Flag - (1 if alarm2 was triggered)
 * bit0 A1F      Alarm 0 Flag - (1 if alarm1 was triggered)
 */
void rtc_set_sreg(uint16_t val)
{
    rtc_set_addr(DS3234_SREG_WRITE, val);
}

/** 
 * \brief Get the value of the RTC status register.
 *
 * \return ret_val Value of the RTC status register.
 */
uint8_t rtc_get_sreg(void)
{
	uint8_t ret_val;
	ret_val = rtc_get_addr(DS3234_SREG_READ);
	return ret_val;
}