/*
	Author: Keenan Burnett

	***********************************************************************
*	FILE NAME:		atomic.h
*
*	PURPOSE:		This file is used to house the incldues, definitions, prototypes for atomic.c
*
*	FILE REFERENCES:		Task.h, FreeRTOS.h
*
*	EXTERNAL VARIABLES:
*
*	EXTERNAL REFERENCES:	Same a File References.
*
*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
*
*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
*	DEVELOPMENT HISTORY:
*	10/16/2015			Created.
*
*/

#include "FreeRTOS.h"
#include "task.h"

void enter_atomic(void);
void exit_atomic(void);

