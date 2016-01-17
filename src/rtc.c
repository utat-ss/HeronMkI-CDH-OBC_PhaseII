/*
	Original Author:          Petre Rodan <petre.rodan@simplex.ro>
	Available from:  https://github.com/rodan/ds3234
	License:         GNU GPLv3
	
    Modified by: Omar Abdeldayem, Keenan Burnett
	***********************************************************************
	*	FILE NAME:		rtc.c
	*
	*	PURPOSE:		DS3234 Real Time Clock functionality through SPI, using the ATSAM3X8E as Master. 
	*
	*	FILE REFERENCES:	rtc.h
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
	*	07/18/2015		Created.
	*
	*	08/08/2015		Setting and reading time from the RTC is now functional.
	*	
	*   08/29/2015      Removed the added century bit from month (unnecessary) and simplified the
	*					timestamp structure to contain only the essential information.
	*
	*					Added support for one of the two alarms on the DS3234. Alarm 2 is configured
	*					to trigger every minute to support the time_update task.
	*
	*	09/27/2015		K: I have added "chip_sel" to each of the spi_master_transfer calls. RTC is to be
	*					used on CS0. Everything else should work just as it did before.
	*					NOTE: We're using variable peripheral select now.
	*
	*	10/03/2015		K: The way in which the SPI devices were connected on the flatsat was a little weird.
	*					Because of this, the RTC is currently connected to CS1, and I am currently using
	*					SPIMEM2 on CS2 for my testing. I changed all the spi_master_transfer calls as such.
	*
	*	11/15/2015		K: I changed all the function headers to the proper format.
	*
	*	DESCRIPTION:	
	*			
	*					Provides the functionality to use the DS3234 as an external RTC using SPI. 
	*					
    *					Retrieving and setting the time/date are currently supported.
	*					
	*					Alarm 1 , SRAM, square wave generator and watchdog timer functionality of the chip
	*					are disabled.
	
 */
#include "rtc.h"

/************************************************************************/
/* DECTOBCD			 		                                            */
/* @Purpose: Decimal to binary code decimal conversion					*/
/************************************************************************/
static uint8_t dectobcd(uint8_t val)
{
	return ((val / 10 * 16) + (val % 10));
}

/************************************************************************/
/* BCDTODEC			 		                                            */
/* @Purpose: Binary coded decimal to decimal conversion					*/
/************************************************************************/
static uint8_t bcdtodec(uint8_t val)
{
	return ((val / 16 * 10) + (val % 16));
}

/************************************************************************/
/* RTC_INIT			 		                                            */
/* @Purpose: Initializes the RTC by configuring the control register and*/
/* setting the time-date to 00:00:00 01/01/0000							*/
/* @param: crtl_reg_val: The byte to set the control register to.		*/
/************************************************************************/
void rtc_init(uint16_t ctrl_reg_val)
{	
    int attempts,x;
	rtc_set_creg(ctrl_reg_val);
	
	attempts = 0; x = -1;
	while(attempts<3 && x<0){
		x = spimem_read(TIME_BASE, absolute_time_arr, 4);	
	}
	if (x<0) {errorREPORT(TIME_TASK_ID, 0, RTC_SPIMEM_R_ERROR, &ctrl_reg_val);}
	
	
	
	// Get the absolute time which may be stored in memory.
	ABSOLUTE_DAY = absolute_time_arr[0];
	CURRENT_HOUR = absolute_time_arr[1];
	CURRENT_MINUTE = absolute_time_arr[2];
	CURRENT_SECOND = absolute_time_arr[3];
	// FAILURE_RECOVERY if this function returns -1.
	struct timestamp initial_time;

	initial_time.sec = CURRENT_SECOND;
	initial_time.minute = CURRENT_MINUTE;
	initial_time.hour = CURRENT_HOUR;
	initial_time.mday = 0x01;
	initial_time.wday = 0x01;
	initial_time.mon = 0x01;
	initial_time.year = 0x00;
	
	rtc_set(initial_time);
	
	rtc_set_a2();
	rtc_clear_a2_flag();
}

/************************************************************************/
/* RTC_SET																*/
/* @Purpose: Set the time and date of the RTC to a specified value.		*/
/* @param: t: the time struct containing the new time/date to update to	*/
/************************************************************************/
void rtc_set(struct timestamp t)
{
	uint8_t time_date[7] = { t.sec, t.minute, t.hour, t.wday, t.mday, t.mon, t.year };
    uint8_t i;
	uint16_t message, addr, data;

    for (i = 0; i < 7; i++) 
	{
		// Convert data and prepare message to send
		addr = i + 0x80;
		data = dectobcd(time_date[i]);
		
		message = (addr << 8) | data;
		spi_master_transfer(&message, 1, 1);	// Chip-Select 1.
    }
}

/************************************************************************/
/* RTC_GET			 		                                            */
/* @Purpose: Retrieves the current time and date from the RTC.			*/
/* @param: t: pointer to an empty timestamp struct that will contain	*/
/* the values retrieved from the RTC.									*/
/************************************************************************/
void rtc_get(struct timestamp *t)
{	
    uint8_t time_date[7];        // second, minute, hour, day of week, day of month, month, year
    uint8_t i, ret_val;
    uint16_t year_full, message;
	
    for (i = 0; i < 7; i++) 
	{
		// Message contains the register address then an empty byte to flush it out
		message = i + 0x00;
		message = message << 8;

		spi_master_transfer(&message, 1, 1);	// Chip-Select 1.
		
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

/************************************************************************/
/* RTC_SET_ADDR		 		                                            */
/* @Purpose: Sets an RTC register to a specfied value.					*/
/* @param: addr: RTC register address									*/
/* @param: val: New register value.										*/
/************************************************************************/
void rtc_set_addr(uint16_t addr, uint16_t val)
{
	uint16_t message = (addr << 8) | val;
	spi_master_transfer(&message, 1, 1);	// Chip-Select 1.
}

/************************************************************************/
/* RTC_GET_ADDR		 		                                            */
/* @Purpose: Gets the value from a register								*/
/* @return: val: value stored in specified register						*/
/************************************************************************/
uint8_t rtc_get_addr(uint16_t addr)
{
	uint8_t val;
	uint16_t message = (uint16_t) addr << 8;
	
	spi_master_transfer(&message, 1, 1);	// Chip-Select 1.
	
	val = (uint8_t) message;	
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

/************************************************************************/
/* RTC_SET_SREG		 		                                            */
/* @Purpose: Sets the status register of the RTC to a specific value	*/
/* @param: Value to write to the status register, each bit correponding	*/
/* to the following:													*/
/* bit7 OSF      Oscillator Stop Flag (if 1 then oscillator has stopped */
/* and date might be innacurate)										*/
/* bit6 BB32kHz  Battery Backed 32kHz output (1 if square wave is needed*/
/* when powered by battery)												*/
/* bit5 CRATE1   Conversion rate 1  temperature compensation rate		*/
/* bit4 CRATE0   Conversion rate 0  temperature compensation rate		*/
/* bit3 EN32kHz  Enable 32kHz output (1 if needed)						*/
/* bit2 BSY      Busy with TCXO functions								*/
/* bit1 A2F      Alarm 1 Flag - (1 if alarm2 was triggered)				*/
/* bit0 A1F      Alarm 0 Flag - (1 if alarm1 was triggered)				*/
/************************************************************************/
void rtc_set_sreg(uint16_t val)
{
    rtc_set_addr(DS3234_SREG_WRITE, val);
}

/************************************************************************/
/* RTC_GET_SREG		 		                                            */
/* @Purpose: Get the value of the RTC status register					*/
/* @return: ret_val: Value of the RTC status register					*/
/************************************************************************/
uint8_t rtc_get_sreg(void)
{
	uint8_t ret_val;
	ret_val = rtc_get_addr(DS3234_SREG_READ);
	return ret_val;
}

/************************************************************************/
/* RTC_SET_A2		 		                                            */
/* @Purpose: Sets the RTC Alarm 2 to trigger every minute				*/
/************************************************************************/
void rtc_set_a2(void)
{
	uint8_t i;
	uint16_t buffer, message;
	
	for (i = 0; i <= 2; i++) 
	{
		buffer = i + 0x8B;

		message = (buffer << 8) | 0x80;
		spi_master_transfer(&message, 1, 1);	// Chip-Select 1.
	}
}

/************************************************************************/
/* RTC_RESET_A2		 		                                            */
/* @Purpose: Clears and resets the RTC Alarm 2 to trigger every minute	*/
/************************************************************************/
void rtc_reset_a2(void)
{
	rtc_set_creg(DS3234_INTCN | DS3234_A2IE);
	rtc_clear_a2_flag();	
}

/************************************************************************/
/* RTC_CLEAR_A2_FLAG 		                                            */
/* @Purpose: Clears the RTC Alarm 2 Flag								*/
/************************************************************************/
void rtc_clear_a2_flag(void)
{
	uint8_t reg_val;
	reg_val = rtc_get_sreg() & ~DS3234_A2F;
	
	rtc_set_sreg(reg_val);
}

/************************************************************************/
/* RTC_TRIGGERED_A2 		                                            */
/* @Purpose: Checks the RTC Alarm 2 flag to see if the alarm has trig'd */
/* @return: 1 == alarm is triggered, 0 == it is not.					*/
/************************************************************************/
uint8_t rtc_triggered_a2(void)
{
	return  rtc_get_sreg() & DS3234_A2F;
}
