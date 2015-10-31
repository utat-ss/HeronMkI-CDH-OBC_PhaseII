/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved
Copyright (C) 2012 Atmel Corporation. All rights reserved.

Edited by: Keenan Burnett
***********************************************************************
*	FILE NAME:		sam3x_ek.h
*
*	PURPOSE:
*	Brief SAM3X-EK Board Definition.
*
*	FILE REFERENCES:	compiler.h, system_sam3x.h, exceptions.h
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
*	BOARD_REV_A and BOARD_REV_B have different pin settings.
*	
*	Important: even though the file name is sam3x_ek, I am actually using pin definitions which 
*	pertain to the Arduino DUE.
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

#ifndef _SAM3X_EK_H_
#define _SAM3X_EK_H_

#include "compiler.h"
#include "system_sam3x.h"		//Initially at .h
#include "exceptions.h"

//I included this:
//#include "pio_sam3x8e.h"


/*#define BOARD_REV_A */
#define BOARD_REV_B

/* ------------------------------------------------------------------------ */

/**
 *  \page sam3x_ek_opfreq "SAM3X-EK - Operating frequencies"
 *  This page lists several definition related to the board operating frequency
 *
 *  \section Definitions
 *  - \ref BOARD_FREQ_*
 *  - \ref BOARD_MCK
 */

/*! Board oscillator settings */
#define BOARD_FREQ_SLCK_XTAL            (32768U)
#define BOARD_FREQ_SLCK_BYPASS          (32768U)
#define BOARD_FREQ_MAINCK_XTAL          (12000000U)
#define BOARD_FREQ_MAINCK_BYPASS        (12000000U)

/*! Master clock frequency */
#define BOARD_MCK                       CHIP_FREQ_CPU_MAX

/* ------------------------------------------------------------------------ */

/**
 * \page sam3x_ek_board_info "SAM3X-EK - Board informations"
 * This page lists several definition related to the board description.
 *
 * \section Definitions
 * - \ref BOARD_NAME
 */

/*! Name of the board */
#define BOARD_NAME "SAM3X-EK"
/*! Board definition */
#define sam3xek
/*! Family definition (already defined) */
#define sam3x
/*! Core definition */
#define cortexm3

/* ------------------------------------------------------------------------ */

/**
 * \page sam3x_ek_piodef "SAM3X-EK - PIO definitions"
 * This pages lists all the pio definitions. The constants
 * are named using the following convention: PIN_* for a constant which defines
 * a single Pin instance (but may include several PIOs sharing the same
 * controller), and PINS_* for a list of Pin instances.
 *
 */

/**
 * \file
 * ADC
 * - \ref PIN_ADC0_AD1
 * - \ref PINS_ADC
 *
 */

/*! ADC_AD1 pin definition. */
#define PIN_ADC0_AD1 {PIO_PA3X1_AD1, PIOA, ID_PIOA, PIO_INPUT, PIO_DEFAULT}
#define PINS_ADC_TRIG  PIO_PA11_IDX
#define PINS_ADC_TRIG_FLAG  (PIO_PERIPH_B | PIO_DEFAULT)
/*! Pins ADC */
#define PINS_ADC PIN_ADC0_AD1

/**
 * \file
 * LEDs
 * - \ref PIN_USER_LED1
 * - \ref PIN_USER_LED2
 * - \ref PIN_USER_LED3
 * - \ref PIN_POWER_LED
 * - \ref PINS_LEDS
 *
 */

/* ------------------------------------------------------------------------ */
/* LEDS                                                                     */
/* ------------------------------------------------------------------------ */
/*! LED #0 pin definition (GREEN). D4 */
#define PIN_USER_LED1   {PIO_PB27, PIOB, ID_PIOB, PIO_OUTPUT_1, PIO_DEFAULT}
/*! LED #1 pin definition (AMBER). D3 */
#define PIN_USER_LED2   {PIO_PC21, PIOB, ID_PIOB, PIO_OUTPUT_1, PIO_DEFAULT}
/*! LED #2 pin definition (BLUE).  D2 */
#define PIN_USER_LED3   {PIO_PC22, PIOA, ID_PIOA, PIO_OUTPUT_1, PIO_DEFAULT}
/*! LED #3 pin definition (RED).   D5 */
#define PIN_POWER_LED   {PIO_PC23, PIOA, ID_PIOA, PIO_OUTPUT_1, PIO_DEFAULT}
/*! LED #4 pin definition (GREEN). DP6 */
#define PIN_USER_LED4   {PIO_PC24, PIOA, ID_PIOA, PIO_OUTPUT_1, PIO_DEFAULT}

#define LED_GREEN     0
#define LED_AMBER     1
#define LED_BLUE      2
#define LED_RED       3

/*! List of all LEDs definitions. */
#define PINS_LEDS   PIN_USER_LED1, PIN_USER_LED2, PIN_USER_LED3, PIN_POWER_LED, PIN_USER_LED4

/*! LED #0 pin definition (BLUE). */
#define LED_0_NAME      "blue LED D2"
#define LED0_GPIO       (PIO_PB27_IDX)
#define LED0_FLAGS      (PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT)

#define PIN_LED_0       {1 << 27, PIOB, ID_PIOB, PIO_OUTPUT_0, PIO_DEFAULT}
#define PIN_LED_0_MASK  (1 << 27)
#define PIN_LED_0_PIO   PIOB
#define PIN_LED_0_ID    ID_PIOB
#define PIN_LED_0_TYPE  PIO_OUTPUT_0
#define PIN_LED_0_ATTR  PIO_DEFAULT

/*! LED #1 pin definition (GREEN). */
#define LED_1_NAME      "green LED D4"
#define LED1_GPIO       (PIO_PC21_IDX)
#define LED1_FLAGS      (PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT)

#define PIN_LED_1       {1 << 21, PIOC, ID_PIOC, PIO_OUTPUT_1, PIO_DEFAULT}
#define PIN_LED_1_MASK  (1 << 21)
#define PIN_LED_1_PIO   PIOC
#define PIN_LED_1_ID    ID_PIOC
#define PIN_LED_1_TYPE  PIO_OUTPUT_1
#define PIN_LED_1_ATTR  PIO_DEFAULT

/*! LED #2 pin detection (AMBER). */
#define LED2_GPIO       (PIO_PC22_IDX)
#define LED2_FLAGS      (PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT)

/*! LED #3 pin detection (power) */
#define LED3_GPIO       (PIO_PC23_IDX)
#define LED3_FLAGS      (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT)

/*! LED #4 pin detection */
#define LED4_GPIO       (PIO_PC24_IDX)
#define LED4_FLAGS      (PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT)
//
//#define PIN_LED_0       {1 << 27, PIOB, ID_PIOB, PIO_OUTPUT_0, PIO_DEFAULT}
//#define PIN_LED_0_MASK  (1 << 27)
//#define PIN_LED_0_PIO   PIOB
//#define PIN_LED_0_ID    ID_PIOB
//#define PIN_LED_0_TYPE  PIO_OUTPUT_0
//#define PIN_LED_0_ATTR  PIO_DEFAULT

/**
 * \file
 * PWMC
 * - \ref PIN_PWMC_PWMH0
 * - \ref PIN_PWMC_PWML0
 * - \ref PIN_PWMC_PWMH1
 * - \ref PIN_PWMC_PWML1
 * - \ref PIN_PWMC_PWMH2
 * - \ref PIN_PWMC_PWML2
 * - \ref PIN_PWMC_PWMH3
 * - \ref PIN_PWMC_PWML3
 * - \ref PIN_PWM_LED0
 * - \ref PIN_PWM_LED1
 * - \ref PIN_PWM_LED2
 * - \ref CHANNEL_PWM_LED0
 * - \ref CHANNEL_PWM_LED1
 * - \ref CHANNEL_PWM_LED2
 *
 */

/* ------------------------------------------------------------------------ */
/* PWM                                                                      */
/* ------------------------------------------------------------------------ */
/*! PWMC PWM0 pin definition: Output High. */
#define PIN_PWMC_PWMH0\
	{PIO_PB12B_PWMH0, PIOB, ID_PIOB, PIO_PERIPH_B, PIO_DEFAULT}
#define PIN_PWMC_PWMH0_TRIG   PIO_PB12_IDX
#define PIN_PWMC_PWMH0_TRIG_FLAG   PIO_PERIPH_B | PIO_DEFAULT
/*! PWMC PWM0 pin definition: Output Low. */
#define PIN_PWMC_PWML0\
	{PIO_PA21B_PWML0, PIOA, ID_PIOA, PIO_PERIPH_B, PIO_DEFAULT}
/*! PWMC PWM1 pin definition: Output High. */
#define PIN_PWMC_PWMH1\
	{PIO_PB13B_PWMH1, PIOB, ID_PIOB, PIO_PERIPH_B, PIO_DEFAULT}
/*! PWMC PWM1 pin definition: Output Low. */
#define PIN_PWMC_PWML1\
	{PIO_PB17B_PWML1, PIOB, ID_PIOB, PIO_PERIPH_B, PIO_DEFAULT}
/*! PWMC PWM2 pin definition: Output High. */
#define PIN_PWMC_PWMH2\
	{PIO_PA13B_PWMH2, PIOA, ID_PIOA, PIO_PERIPH_B, PIO_DEFAULT}
/*! PWMC PWM2 pin definition: Output Low. */
#define PIN_PWMC_PWML2\
	{PIO_PA20B_PWML2, PIOA, ID_PIOA, PIO_PERIPH_B, PIO_DEFAULT}
/*! PWMC PWM3 pin definition: Output High. */
#define PIN_PWMC_PWMH3\
	{PIO_PA9B_PWMH3, PIOA, ID_PIOA, PIO_PERIPH_B, PIO_DEFAULT}
/*! PWMC PWM3 pin definition: Output Low. */
#define PIN_PWMC_PWML3\
	{PIO_PC8B_PWML3, PIOC, ID_PIOC, PIO_PERIPH_B, PIO_DEFAULT}
/*! PWM pins definition for LED0 */
#define PIN_PWM_LED0 PIN_PWMC_PWMH0, PIN_PWMC_PWML0
/*! PWM pins definition for LED1 */
#define PIN_PWM_LED1 PIN_PWMC_PWMH1, PIN_PWMC_PWML1
/*! PWM pins definition for LED2 */
#define PIN_PWM_LED2 PIN_PWMC_PWMH2, PIN_PWMC_PWML2
/*! PWM channel for LED0 */
#define CHANNEL_PWM_LED0 0
/*! PWM channel for LED1 */
#define CHANNEL_PWM_LED1 1
/*! PWM channel for LED2 */
#define CHANNEL_PWM_LED2 2

/*! PWM LED0 pin definitions. */
#define PIN_PWM_LED0_GPIO    PIO_PB27_IDX
#define PIN_PWM_LED0_FLAGS   (PIO_PERIPH_B | PIO_DEFAULT)
#define PIN_PWM_LED0_CHANNEL PWM_CHANNEL_1

/*! PWM LED1 pin definitions. */
#define PIN_PWM_LED1_GPIO    PIO_PC21_IDX
#define PIN_PWM_LED1_FLAGS   (PIO_PERIPH_B | PIO_DEFAULT)
#define PIN_PWM_LED1_CHANNEL PWM_CHANNEL_0

/*! PWM LED2 pin definitions. */
#define PIN_PWM_LED2_GPIO    PIO_PC22_IDX
#define PIN_PWM_LED2_FLAGS   (PIO_PERIPH_B | PIO_DEFAULT)

/**
 * \file
 * SPI
 * - \ref PIN_SPI_MISO
 * - \ref PIN_SPI_MOSI
 * - \ref PIN_SPI_SPCK
 * - \ref PINS_SPI
 * - \ref PIN_SPI_NPCS0
 * - \ref PIN_SPI1_MISO
 * - \ref PIN_SPI1_MOSI
 * - \ref PIN_SPI1_SPCK
 * - \ref PINS_SPI1
 * - \ref PIN_SPI1_NPCS0
 *
 */

/* ------------------------------------------------------------------------ */
/* SPI                                                                      */
/* ------------------------------------------------------------------------ */
/*! SPI MISO pin definition. */
#define PIN_SPI0_MISO\
	{PIO_PA25A_SPI0_MISO, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
/*! SPI MOSI pin definition. */
#define PIN_SPI0_MOSI\
	{PIO_PA26A_SPI0_MOSI, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
/*! SPI SPCK pin definition. */
#define PIN_SPI0_SPCK\
	{PIO_PA27A_SPI0_SPCK, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
/*! SPI chip select pin definition. */
#define PIN_SPI0_NPCS0\
	{PIO_PA28A_SPI0_NPCS0, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
/*! List of SPI pin definitions (MISO, MOSI & SPCK). */
#define PINS_SPI0        PIN_SPI0_MISO, PIN_SPI0_MOSI, PIN_SPI0_SPCK

/*! SPI0 MISO pin definition. */
#define SPI0_MISO_GPIO        (PIO_PA25_IDX)
#define SPI0_MISO_FLAGS       (PIO_PERIPH_A | PIO_DEFAULT)
/*! SPI0 MOSI pin definition. */
#define SPI0_MOSI_GPIO        (PIO_PA26_IDX)
#define SPI0_MOSI_FLAGS       (PIO_PERIPH_A | PIO_DEFAULT)
/*! SPI0 SPCK pin definition. */
#define SPI0_SPCK_GPIO        (PIO_PA27_IDX)
#define SPI0_SPCK_FLAGS       (PIO_PERIPH_A | PIO_DEFAULT)

/*! SPI0 chip select 0 pin definition. (Only one configuration is possible) */
#define SPI0_NPCS0_GPIO            (PIO_PA28_IDX)
#define SPI0_NPCS0_FLAGS           (PIO_PERIPH_A | PIO_DEFAULT)
/*! SPI0 chip select 1 pin definition. (multiple configurations are possible) */
#define SPI0_NPCS1_PA29_GPIO       (PIO_PA29_IDX)
#define SPI0_NPCS1_PA29_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
#define SPI0_NPCS1_PB20_GPIO       (PIO_PB20_IDX)
#define SPI0_NPCS1_PB20_FLAGS      (PIO_PERIPH_B | PIO_DEFAULT)
/*! SPI0 chip select 2 pin definition. (multiple configurations are possible) */
#define SPI0_NPCS2_PA30_GPIO       (PIO_PA30_IDX)
#define SPI0_NPCS2_PA30_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
#define SPI0_NPCS2_PB21_GPIO       (PIO_PB21_IDX)
#define SPI0_NPCS2_PB21_FLAGS      (PIO_PERIPH_B | PIO_DEFAULT)
/*! SPI0 chip select 3 pin definition. (multiple configurations are possible) */
#define SPI0_NPCS3_PA31_GPIO       (PIO_PA31_IDX)
#define SPI0_NPCS3_PA31_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
#define SPI0_NPCS3_PB23_GPIO       (PIO_PB23_IDX)
#define SPI0_NPCS3_PB23_FLAGS      (PIO_PERIPH_B | PIO_DEFAULT)

/*		Write-Protect and Hold Pins	Definition		*/
#define SPI0_MEM2_WP				(PIO_PA28_IDX)
#define SPI0_MEM2_WP_FLAGS			(PIO_PERIPH_A| PIO_DEFAULT)
#define SPI0_MEM2_HOLD				(PIO_PB0_IDX)
#define SPI0_MEM2_HOLD_FLAGS		(PIO_PERIPH_B| PIO_DEFAULT)

#define SPI0_MEM1_WP				(PIO_PB19_IDX)
#define SPI0_MEM1_WP_FLAGS			(PIO_PERIPH_B| PIO_DEFAULT)
#define	SPI0_MEM1_HOLD				(PIO_PA21_IDX)
#define SPI0_MEM1_HOLD_FLAGS		(PIO_PERIPH_A| PIO_DEFAULT)			

/**
 * \file
 * SSC
 * - \ref PIN_SSC_TD
 * - \ref PIN_SSC_TK
 * - \ref PIN_SSC_TF
 * - \ref PINS_SSC_CODEC
 *
 */

/* ------------------------------------------------------------------------ */
/* SSC                                                                      */
/* ------------------------------------------------------------------------ */
/*! SSC pin Transmitter Data (TD) */
#define PIN_SSC_TD      {PIO_PA16B_TD, PIOA, ID_PIOA, PIO_PERIPH_B, PIO_DEFAULT}
/*! SSC pin Transmitter Clock (TK) */
#define PIN_SSC_TK      {PIO_PA14B_TK, PIOA, ID_PIOA, PIO_PERIPH_B, PIO_DEFAULT}
/*! SSC pin Transmitter FrameSync (TF) */
#define PIN_SSC_TF      {PIO_PA15B_TF, PIOA, ID_PIOA, PIO_PERIPH_B, PIO_DEFAULT}

/*! SSC pin Receiver Data (RD) */
#define PIN_SSC_RD      {PIO_PB18A_RD, PIOB, ID_PIOB, PIO_PERIPH_A, PIO_DEFAULT}
/*! SSC pin Receiver Clock (RK) */
#define PIN_SSC_RK      {PIO_PB19A_RK, PIOB, ID_PIOB, PIO_PERIPH_A, PIO_DEFAULT}
/*! SSC pin Receiver FrameSync (RF) */
#define PIN_SSC_RF      {PIO_PB17A_RF, PIOB, ID_PIOB, PIO_PERIPH_A, PIO_DEFAULT}

/*! SSC pins definition for codec. */
#define PINS_SSC_CODEC  PIN_SSC_TD, PIN_SSC_TK, PIN_SSC_TF,\
	PIN_SSC_RD, PIN_SSC_RK, PIN_SSC_RF

/**
 * \file
 * PCK0
 * - \ref PIN_PCK0
 *
 */

/* ------------------------------------------------------------------------ */
/* PCK                                                                      */
/* ------------------------------------------------------------------------ */
/*! PCK0 */
#define PIN_PCK0        (PIO_PB22_IDX)
#define PIN_PCK0_FLAGS  (PIO_PERIPH_B | PIO_DEFAULT)

#define PIN_PCK_0_MASK  PIO_PB22
#define PIN_PCK_0_PIO   PIOB
#define PIN_PCK_0_ID    ID_PIOB
#define PIN_PCK_0_TYPE  PIO_PERIPH_B
#define PIN_PCK_0_ATTR  PIO_DEFAULT

/**
 * \file
 * UART
 * - \ref PINS_UART
 *
 */

/* ------------------------------------------------------------------------ */
/* UART                                                                     */
/* ------------------------------------------------------------------------ */
/*! UART pins (UTXD0 and URXD0) definitions, PA8,9. */
#define PINS_UART        (PIO_PA8A_URXD | PIO_PA9A_UTXD)
#define PINS_UART_FLAGS  (PIO_PERIPH_A | PIO_DEFAULT)

#define PINS_UART_MASK (PIO_PA8A_URXD | PIO_PA9A_UTXD)
#define PINS_UART_PIO  PIOA
#define PINS_UART_ID   ID_PIOA
#define PINS_UART_TYPE PIO_PERIPH_A
#define PINS_UART_ATTR PIO_DEFAULT

/**
 * \file
 * USART0
 * - \ref PIN_USART0_RXD
 * - \ref PIN_USART0_TXD
 * - \ref PIN_USART0_CTS
 * - \ref PIN_USART0_RTS
 * - \ref PIN_USART0_SCK
 *
 * - \ref PIN_USART0_EN
 */

/* ------------------------------------------------------------------------ */
/* USART0                                                                   */
/* ------------------------------------------------------------------------ */
/*! USART0 pin RX */
#define PIN_USART0_RXD\
	{PIO_PA10A_RXD0, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
#define PIN_USART0_RXD_IDX        (PIO_PA10_IDX)
#define PIN_USART0_RXD_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
/*! USART0 pin TX */
#define PIN_USART0_TXD\
	{PIO_PA11A_TXD0, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
#define PIN_USART0_TXD_IDX        (PIO_PA11_IDX)
#define PIN_USART0_TXD_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
/*! USART0 pin CTS */
#define PIN_USART0_CTS\
	{PIO_PB26A_CTS0, PIOB, ID_PIOB, PIO_PERIPH_A, PIO_DEFAULT}
#define PIN_USART0_CTS_IDX        (PIO_PB26_IDX)
#define PIN_USART0_CTS_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
/*! USART0 pin RTS */
#define PIN_USART0_RTS\
	{PIO_PB25A_RTS0, PIOB, ID_PIOB, PIO_PERIPH_A, PIO_DEFAULT}
#define PIN_USART0_RTS_IDX        (PIO_PB25_IDX)
#define PIN_USART0_RTS_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
/*! USART0 pin SCK */
#define PIN_USART0_SCK\
	{PIO_PA17B_SCK0, PIOA, ID_PIOA, PIO_PERIPH_B, PIO_DEFAULT}
#define PIN_USART0_SCK_IDX        (PIO_PA17_IDX)
#define PIN_USART0_SCK_FLAGS      (PIO_PERIPH_B | PIO_DEFAULT)

/**
 * \file
 * USART1
 * - \ref PIN_USART1_RXD
 * - \ref PIN_USART1_TXD
 * - \ref PIN_USART1_CTS
 * - \ref PIN_USART1_RTS
 * - \ref PIN_USART1_SCK
 *
 */

/* ------------------------------------------------------------------------ */
/* USART1                                                                   */
/* ------------------------------------------------------------------------ */
/*! USART1 pin RX */
#define PIN_USART1_RXD\
	{PIO_PA12A_RXD1, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
#define PIN_USART1_RXD_IDX        (PIO_PA12_IDX)
#define PIN_USART1_RXD_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
/*! USART1 pin TX */
#define PIN_USART1_TXD\
	{PIO_PA13A_TXD1, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
#define PIN_USART1_TXD_IDX        (PIO_PA13_IDX)
#define PIN_USART1_TXD_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
/*! USART1 pin CTS */
#define PIN_USART1_CTS\
	{PIO_PA15A_CTS1, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
#define PIN_USART1_CTS_IDX        (PIO_PA15_IDX)
#define PIN_USART1_CTS_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
/*! USART1 pin RTS */
#define PIN_USART1_RTS\
	{PIO_PA14A_RTS1, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
#define PIN_USART1_RTS_IDX        (PIO_PA14_IDX)
#define PIN_USART1_RTS_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
/*! USART1 pin SCK */
#define PIN_USART1_SCK\
	{PIO_PA16A_SCK1, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
#define PIN_USART1_SCK_IDX        (PIO_PA16_IDX)
#define PIN_USART1_SCK_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)

/**
 * \file
 * USART3
 * - \ref PIN_USART3_RXD
 * - \ref PIN_USART3_TXD
 * - \ref PIN_USART3_CTS
 * - \ref PIN_USART3_RTS
 * - \ref PIN_USART3_SCK
 *
 */

/* ------------------------------------------------------------------------ */
/* USART3                                                                   */
/* ------------------------------------------------------------------------ */
/*! USART1 pin RX */
#define PIN_USART3_RXD\
	{PIO_PD5B_RXD3, PIOD, ID_PIOD, PIO_PERIPH_B, PIO_DEFAULT}
#define PIN_USART3_RXD_IDX        (PIO_PD5_IDX)
#define PIN_USART3_RXD_FLAGS      (PIO_PERIPH_B | PIO_DEFAULT)
/*! USART1 pin TX */
#define PIN_USART3_TXD\
	{PIO_PD4B_TXD3, PIOD, ID_PIOD, PIO_PERIPH_B, PIO_DEFAULT}
#define PIN_USART3_TXD_IDX        (PIO_PD4_IDX)
#define PIN_USART3_TXD_FLAGS      (PIO_PERIPH_B | PIO_DEFAULT)
/*! USART1 pin CTS */
#define PIN_USART3_CTS\
	{PIO_PF4A_CTS3, PIOF, ID_PIOF, PIO_PERIPH_A, PIO_DEFAULT}
#define PIN_USART3_CTS_IDX        (PIO_PF4_IDX)
#define PIN_USART3_CTS_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
/*! USART1 pin RTS */
#define PIN_USART3_RTS\
	{PIO_PF5A_RTS3, PIOF, ID_PIOF, PIO_PERIPH_A, PIO_DEFAULT}
#define PIN_USART3_RTS_IDX        (PIO_PF5_IDX)
#define PIN_USART3_RTS_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
/*! USART1 pin SCK */
#define PIN_USART3_SCK\
	{PIO_PE16B_SCK3, PIOE, ID_PIOE, PIO_PERIPH_B, PIO_DEFAULT}
#define PIN_USART3_SCK_IDX        (PIO_PE16_IDX)
#define PIN_USART3_SCK_FLAGS      (PIO_PERIPH_B | PIO_DEFAULT)

/* ------------------------------------------------------------------------ */

/**
 * \file
 * CAN
 * \ref PIN_CAN0_TRANSCEIVER_RXEN
 * \ref PIN_CAN0_TRANSCEIVER_RS
 * \ref PIN_CAN0_TXD
 * \ref PIN_CAN0_RXD
 * \ref PINS_CAN0
 *
 * \ref PIN_CAN1_TRANSCEIVER_RXEN
 * \ref PIN_CAN1_TRANSCEIVER_RS
 * \ref PIN_CAN1_TXD
 * \ref PIN_CAN1_RXD
 * \ref PINS_CAN1
 */

/* ------------------------------------------------------------------------ */
/* CAN                                                                      */
/* ------------------------------------------------------------------------ */

/*! CAN0 RXEN: Select input for high speed mode or ultra low current sleep mode */
#define PIN_CAN0_TRANSCEIVER_RXEN\
	{ PIO_PB21, PIOB, ID_PIOB, PIO_OUTPUT_1, PIO_DEFAULT }

/*! CAN0 RS: Select input for high speed mode or low-current standby mode */
#define PIN_CAN0_TRANSCEIVER_RS\
	{ PIO_PB20, PIOB, ID_PIOB, PIO_OUTPUT_0, PIO_DEFAULT }

/*! CAN0 TXD: Transmit data input */
#define PIN_CAN0_TXD\
	{ PIO_PA0A_CANTX0, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }

/*! CAN0 RXD: Receive data output */
#define PIN_CAN0_RXD\
	{ PIO_PA1A_CANRX0, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT }

/*! List of all CAN0 definitions. */
#define PINS_CAN0    PIN_CAN0_TXD, PIN_CAN0_RXD

/** CAN0 transceiver PIN RS. */
#define PIN_CAN0_TR_RS_IDX        PIO_PB20_IDX
#define PIN_CAN0_TR_RS_FLAGS      (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT)

/** CAN0 transceiver PIN EN. */
#define PIN_CAN0_TR_EN_IDX        PIO_PB21_IDX
#define PIN_CAN0_TR_EN_FLAGS      (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT)

/** CAN0 PIN RX. */
#define PIN_CAN0_RX_IDX           (PIO_PA1_IDX)
#define PIN_CAN0_RX_FLAGS         (PIO_PERIPH_A | PIO_DEFAULT)

/** CAN0 PIN TX. */
#define PIN_CAN0_TX_IDX           (PIO_PA0_IDX)
#define PIN_CAN0_TX_FLAGS         (PIO_PERIPH_A | PIO_DEFAULT)

/*! CAN1 RXEN: Select input for high speed mode or ultra low current sleep mode */
#define PIN_CAN1_TRANSCEIVER_RXEN\
	{ PIO_PE16, PIOE, ID_PIOE, PIO_OUTPUT_1, PIO_DEFAULT }

/*! CAN1 RS: Select input for high speed mode or low-current standby mode */
#define PIN_CAN1_TRANSCEIVER_RS\
	{ PIO_PE15, PIOE, ID_PIOE, PIO_OUTPUT_0, PIO_DEFAULT }

/*! CAN1 TXD: Transmit data input */
#define PIN_CAN1_TXD\
	{ PIO_PB14A_CANTX1, PIOB, ID_PIOB, PIO_PERIPH_A, PIO_DEFAULT }

/*! CAN1 RXD: Receive data output */
#define PIN_CAN1_RXD\
	{ PIO_PB15A_CANRX1, PIOB, ID_PIOB, PIO_PERIPH_A, PIO_DEFAULT }

/*! List of all CAN1 definitions. */
#define PINS_CAN1    PIN_CAN1_TXD, PIN_CAN1_RXD

/** CAN1 transceiver PIN RS. */
//#define PIN_CAN1_TR_RS_IDX        PIO_PE15_IDX
//#define PIN_CAN1_TR_RS_FLAGS      (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT)
//
///** CAN1 transceiver PIN EN. */
//#define PIN_CAN1_TR_EN_IDX        PIO_PE16_IDX
//#define PIN_CAN1_TR_EN_FLAGS      (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT)

/** CAN1 PIN RX. */
#define PIN_CAN1_RX_IDX           (PIO_PB15_IDX)
#define PIN_CAN1_RX_FLAGS         (PIO_PERIPH_A | PIO_DEFAULT)

/** CAN1 PIN TX. */
#define PIN_CAN1_TX_IDX           (PIO_PB14_IDX)
#define PIN_CAN1_TX_FLAGS         (PIO_PERIPH_A | PIO_DEFAULT)

/**
 * \file
 * TWI
 * - \ref PIN_TWI_TWD0
 * - \ref PIN_TWI_TWCK0
 * - \ref PINS_TWI0
 * - \ref PIN_TWI_TWD1
 * - \ref PIN_TWI_TWCK1
 * - \ref PINS_TWI1
 *
 */

/* ------------------------------------------------------------------------ */
/* TWI                                                                      */
/* ------------------------------------------------------------------------ */
/*! TWI0 data pin */
#define PIN_TWI_TWD0\
	{PIO_PA17A_TWD0, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
/*! TWI0 clock pin */
#define PIN_TWI_TWCK0\
	{PIO_PA18A_TWCK0, PIOA, ID_PIOA, PIO_PERIPH_A, PIO_DEFAULT}
/*! TWI0 pins */
#define PINS_TWI0      PIN_TWI_TWD0, PIN_TWI_TWCK0

/*! TWI0 pins definition */
#define TWI0_DATA_GPIO   PIO_PA17_IDX
#define TWI0_DATA_FLAGS  (PIO_PERIPH_A | PIO_DEFAULT)
#define TWI0_CLK_GPIO    PIO_PA18_IDX
#define TWI0_CLK_FLAGS   (PIO_PERIPH_A | PIO_DEFAULT)

/*! TWI1 data pin */
#define PIN_TWI_TWD1\
	{PIO_PB12A_TWD1, PIOB, ID_PIOB, PIO_PERIPH_A, PIO_DEFAULT}
/*! TWI1 clock pin */
#define PIN_TWI_TWCK1\
	{PIO_PB13A_TWCK1, PIOB, ID_PIOB, PIO_PERIPH_A, PIO_DEFAULT}
/*! TWI1 pins */
#define PINS_TWI1      PIN_TWI_TWD1, PIN_TWI_TWCK1

/*! TWI1 pins definition */
#define TWI1_DATA_GPIO   PIO_PB12_IDX
#define TWI1_DATA_FLAGS  (PIO_PERIPH_A | PIO_DEFAULT)
#define TWI1_CLK_GPIO    PIO_PB13_IDX
#define TWI1_CLK_FLAGS   (PIO_PERIPH_A | PIO_DEFAULT)

/* ------------------------------------------------------------------------ */

#endif  /* _SAM3X_EK_H_ */

