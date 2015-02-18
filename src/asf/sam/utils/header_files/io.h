/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved
Copyright (C) 2012 Atmel Corporation. All rights reserved.
***********************************************************************
*	FILE NAME:		io.h
*
*	PURPOSE:
*	Arch file for SAM.
*
*	FILE REFERENCES:	sam3xa.h
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
*	DESCRIPTION:	Not Required.
*
*	DEVELOPMENT HISTORY:
*	11/29/2014			Header Changed.
*
*/

#ifndef _SAM_IO_
#define _SAM_IO_

/* SAM3 family */

/* SAM3S series */
#if (SAM3S)
# if (SAM3S8 || SAM3SD8)
#  include "sam3s8.h"
# else
#  include "sam3s.h"
# endif
#endif

/* SAM3U series */
#if (SAM3U)
#  include "sam3u.h"
#endif

/* SAM3N series */
#if (SAM3N)
#  include "sam3n.h"
#endif

/* SAM3XA series */
#if (SAM3XA)
#  include "sam3xa.h"
#endif

/* SAM4S series */
#if (SAM4S)
#  include "sam4s.h"
#endif

#endif /* _SAM_IO_ */

