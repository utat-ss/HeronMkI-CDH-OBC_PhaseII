/*
	Author: Keenan Burnett

	***********************************************************************
*	FILE NAME:		atomic.c
*
*	PURPOSE:		This file is used to house the functions which will allow tasks to perform
*					certainly actions "atomically", or "uninterrupted".
*
*					Really, the tasks can still be interrupted if the watchdog timer resets the
*					system, or if something external triggers a hard-reset.
*
*	FILE REFERENCES:		atomic.h
*
*	NOTES:			Please ask for permission from the CDH team before using these functions.
*
*					Also, DO NOT use these functions within an interrupt service routine.
*
*					You MUST call exit() after you call enter()
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
*	DEVELOPMENT HISTORY:
*	10/16/2015			Created.
*
*	DESCRIPTION:
*
*/

#include "atomic.h"

/************************************************************************/
/* ENTER_ATOMIC	& EXIT_ATOMIC                                           */
/* @return: None.														*/
/* @purpose: This function disables interrupts and allows the user to	*/
/* run an operation without worry of being interrupted by anything		*/
/* except the watchdog timer or a hard reset.							*/
/*																		*/
/* Ex:																	*/
/* enter_atomic();														*/
/* // Your uninterrupted operation ...									*/
/* exit_atomic();														*/
/************************************************************************/
void enter_atomic(void)
{
	taskENTER_CRITICAL();
	return;
}

void exit_atomic(void)
{
	taskEXIT_CRITICAL();
	return;
}

