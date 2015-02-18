/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved
Copyright (C) 2012 Atmel Corporation. All rights reserved.
***********************************************************************
*	FILE NAME:		stringz.h
*
*	PURPOSE:
*	Preprocessor stringizing utils.
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
*	None.
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*	None.
*
*	DESCRIPTION:	
*   Stringize a preprocessing token, this token being allowed to be \#defined.
*
*   May be used only within macros with the token passed as an argument if the token is \#defined.
*
*   For example, writing STRINGZ(PIN) within a macro \#defined by PIN_NAME(PIN)
*   and invoked as PIN_NAME(PIN0) with PIN0 \#defined as A0 is equivalent to
*   writing "A0".
*
*	DEVELOPMENT HISTORY:
*	11/29/2014			Header Changed.
*
*/

#ifndef _STRINGZ_H_
#define _STRINGZ_H_

/**
 * \defgroup group_sam_utils_stringz Preprocessor - Stringize
 *
 * \ingroup group_sam_utils
 *
 * \{
 */

/*! \brief Stringize.
 *
 * Stringize a preprocessing token, this token being allowed to be \#defined.
 *
 * May be used only within macros with the token passed as an argument if the token is \#defined.
 *
 * For example, writing STRINGZ(PIN) within a macro \#defined by PIN_NAME(PIN)
 * and invoked as PIN_NAME(PIN0) with PIN0 \#defined as A0 is equivalent to
 * writing "A0".
 */
#define STRINGZ(x)                                #x

/*! \brief Absolute stringize.
 *
 * Stringize a preprocessing token, this token being allowed to be \#defined.
 *
 * No restriction of use if the token is \#defined.
 *
 * For example, writing ASTRINGZ(PIN0) anywhere with PIN0 \#defined as A0 is
 * equivalent to writing "A0".
 */
#define ASTRINGZ(x)                               STRINGZ(x)

/**
 * \}
 */

#endif  // _STRINGZ_H_

