/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved
Copyright (C) 2012 Atmel Corporation. All rights reserved.
***********************************************************************
*	FILE NAME:		status_codes.h
*
*	PURPOSE:
*	Status code definitions.
*
*	FILE REFERENCES:	None.
*
*	EXTERNAL VARIABLES:		None that I'm aware of.
*
*	EXTERNAL REFERENCES:	Many, see dependencies diagram on dropbox.
*
*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
*
*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None.
*
*	NOTES:
*	Status code that may be returned by shell commands and protocol
*	implementations.
*
*	Any change to these status codes and the corresponding
*   message strings is strictly forbidden. New codes can be added,
*   however, but make sure that any message string tables are updated
*   at the same time.
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*	None.
*
*	DESCRIPTION:
*	This file defines various status codes returned by functions,
*	indicating success or failure as well as what kind of failure.
*
*	DEVELOPMENT HISTORY:
*	11/29/2014			Header Changed.
*
*/

#ifndef STATUS_CODES_H_INCLUDED
#define STATUS_CODES_H_INCLUDED

enum status_code {
	STATUS_OK		    =     0, //!< Success
	ERR_IO_ERROR		=    -1, //!< I/O error
	ERR_FLUSHED		    =    -2, //!< Request flushed from queue
	ERR_TIMEOUT		    =    -3, //!< Operation timed out
	ERR_BAD_DATA		=    -4, //!< Data integrity check failed
	ERR_PROTOCOL		=    -5, //!< Protocol error
	ERR_UNSUPPORTED_DEV	=    -6, //!< Unsupported device
	ERR_NO_MEMORY		=    -7, //!< Insufficient memory
	ERR_INVALID_ARG		=    -8, //!< Invalid argument
	ERR_BAD_ADDRESS		=    -9, //!< Bad address
	ERR_BUSY		    =   -10, //!< Resource is busy
	ERR_BAD_FORMAT      =   -11, //!< Data format not recognized

	/**
	 * \brief Operation in progress
	 *
	 * This status code is for driver-internal use when an operation
	 * is currently being performed.
	 *
	 * \note Drivers should never return this status code to any
	 * callers. It is strictly for internal use.
	 */
	OPERATION_IN_PROGRESS	= -128,
};

typedef enum status_code status_code_t;

#endif /* STATUS_CODES_H_INCLUDED */

