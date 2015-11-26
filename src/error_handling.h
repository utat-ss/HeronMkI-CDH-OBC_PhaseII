/*
Author: Keenan Burnett
***********************************************************************
* FILE NAME: error_handling.h
*
* PURPOSE:
* This file is to be used for the includes, prototypes, and definitions related to error_handling.c
*
* EXTERNAL VARIABLES:
*
* EXTERNAL REFERENCES: Same a File References.
*
* ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: None yet.
*
* ASSUMPTIONS, CONSTRAINTS, CONDITIONS: None
*
* NOTES:
*
* REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*
* DEVELOPMENT HISTORY:
* 11/26/2015		Created.
*
*/

#include "global_var.h"

/* Global signals tasks can wait on during High-Sev errors */
uint8_t hk_fdir_signal;
uint8_t time_fdir_signal;
uint8_t coms_fdir_signal;
uint8_t eps_fdir_signal;
uint8_t pay_fdir_signal;
uint8_t opr_fdir_signal;
uint8_t sched_fdir_signal;
uint8_t wdt_fdir_signal;
uint8_t mem_fdir_signal;
uint8_t high_error_array[152];
uint8_t low_error_array[152];
// SSMs will have their own local variables for doing this
// can_func.c will not be allowed to block on any signal, only error reports.

// Note that currently acquired mutex locks should be released during the waiting process.

