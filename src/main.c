/*
FreeRTOS V8.1.2 - Copyright (C) 2014 Real Time Engineers Ltd.
All rights reserved

Author: Keenan Burnett
***********************************************************************
*	FILE NAME:		main.c
*
*	PURPOSE:
*	This is the main program that is first called when the program is
*	loaded onto the ATSAM3X. prvSetupHardware() calls several other
*	subroutines which are used for initialization.
*
*	FILE REFERENCES:	stdio.h, FreeRTOS.h, task.h, partest.h, asf.h
*			(By extension, almost everything is linked to main.c)
*
*	EXTERNAL VARIABLES:
*
*	EXTERNAL REFERENCES:	(Many), see the dependencies diagram on the dropbox.
*
*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: 
*
*	Sometimes the WDT does not run out even when it is initialized, the problem
*	seems to be specific to this program only. NEEDS TO BE DEALT WITH.
*
*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
*
*	NOTES:
*	Within conf_board.h, CONF_BOARD_KEEP_WATCHDOG_AT_INIT can either be defined
*	or commented out.
*
*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:
*	New tasks should be written to use as much of CMSIS as possible. The ASF and
*	FreeRTOS API libraries should also be used whenever possible to make the program
*	more portable.

*	DEVELOPMENT HISTORY:
*	11/15/2014		Set maincreate_SIMPLE_BLINKY_DEMO_ONLY to 1.
*
*	11/16/2014		Changed how LED's were toggled before main_blinky() was called.
*
*	11/30/2014		Ported the watchdog timer example over to the main program.
*
*	12/20/2014		Ported the CAN example over to the main program.
*					Changed maincreate_SIMPLE_BLINKY_DEMO_ONLY to PROGRAM_CHOICE.
*
*	12/21/2014		Added can_test0.c to the main program folder, this is another test
*					program which can be run to test CAN functionality.
*
*					Changed PROGRAM_CHOICE to PROGRAM_CHOICE.
*
*	12/24/2014		Included can_test1.h which corresponds to program choice can_test1.
*
*	12/27/2014		PROGRAM_CHOICE is currently 4 => can_test1().
*
*	12/31/2014		Added rtt.c and rtt.h to the project which shall be used for the purpose of housekeeping.
*
*	01/01/2015		PROGRAM_CHOICE is currently 5 => rtt().
*
*	01/02/2015		can_initialize is now a part of prvSetupHardware()
*
*	01/03/2015		housekeep_test.c has been added to the project and is now working. CAN can now be considered
*					operation as regular messages and remote frames can be sent between controllers 0 and 1.
*
*					PROGRAM_CHOICE is currently 6 => housekeep_test()
*
*	01/24/2015		stk600_test0.c has now been added to the project in order to test communication with
*					our newly acquired development board which we are using to simulate a subsystem MCU.
*
*					PROGRAM_CHOICE is currently 7 => stk600_test0()
*
*	02/01/2015		Today I have been working on getting both sending and receiving to work between the STk600
*					and the Arduino DUE.
*
*					I have created a new test file called command_test.c which I am using to test some CAN API 
*					functions that I have written.
*
*					PROGRAM_CHOICE is now set to 8 in order to run command_test()
*
*	02/17/2015		Today I'm working on getting sending and receiving working between the STK600 and the Arduino DUE.
*
*					I am also working on getting housekeeping to work reliably with the subsystem micro which means
*					getting remote messages working over the CAN bus (these already work between CAN0 and CAN1).
*
*	02/18/2015		Today I got housekeeping working with the STK600 and have now created two repositories on which I 
*					will be working. This repository (PhaseII) shall be used as a prototype for the actual flight 
*					software which shall be run on our satellite.
*
*					I am now removing several test programs from this repository.
*
*	07/12/2015		Instead of jumping right into normal operation, we will first enter into what we are calling 'safe mode'
*					We start off by executing the least amount of code possible in order to get the OBC into a state where
*					it is waiting for a CAN message that will allow it to enter into regular operation. In main(), the first
*					function that will be executed is now safe_mode().
*
*	10/03/2015		I am adding spimem_initialize to the list of initialization procedures in prvSetupHardware().
*
*	10/07/2015		Changed Can1_Mutex -> Can0_Mutex, Added Spi0_Mutex to the list of mutexes being initialized.
*
*					Moved the initialization of mutexes into safe_mode().
*
*	10/15/2015		memory_wash and wdt_reset are being added as tasks which are to be created in main.c
*
*	11/07/2015		time_update and memory_wash are now called time_manage and memory_manage.
*
*					I have added the task obc_packet_router in order to handle the routing/creation/decoding of PUS packets
*					This task will have a priority of 5 (same as the soon to be FDIR task)
*
*	11/10/2015		I moved the initialization of Queues and Global variables into main.c
*
*	12/06/2015		I updated all the functions that create tasks so that they return their respective task handles,
*					these are going to be imported for the fdir task to be able to kill running tasks and restart them.
*
*	DESCRIPTION:
*	This is the 'main' file for our program which will run on the OBC.
*	main.c is called from the reset handler and will initialize hardware,
*	create tasks and initialize interrupts.
*	The creation of tasks is segregated into separate files so that
*	logically related tasks are separated.
*
*/

/* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/*LED (ParTest) initialization */
#include "partest.h"

/* Atmel library includes. */
#include <asf.h>

#include "can_func.h"

#include "usart_func.h"

#include "spi_func.h"

#include "rtc.h"

uint32_t data_reg[2];

#include "global_var.h"

#include "spimem.h"

#include "checksum.h"

#include "error_handling.h"

/* Set up the hardware ready to run the program. */
static void prvSetupHardware(void);
/*	Initialize mutexes and semaphores to be used by the programs  */
static void prvInitializeMutexes(void);
/*  Initialize the priorities of various interrupts on the Cortex-M3 System */
static void prvInitializeInterruptPriorities(void);
/*	This is the initial state of operation. Here we wait for go-ahead from a groundstation.	*/
static void safe_mode(void);
/* Initializes FIFOs which are used for routing CAN messages and/or PUS packets to tasks	*/
static void prvInitializeFifos(void);
/* Initializes Global variables used for synchronization and signaling to processes.		*/
static void prvInitializeGlobalVars(void);

/* External functions used to create and encapsulate different tasks*/
extern void my_blink(void);
extern void command_loop(void);
extern TaskHandle_t housekeep(void);
extern void data_test(void);
extern TaskHandle_t time_manage(void);
extern TaskHandle_t eps(void);
extern TaskHandle_t coms(void);
extern TaskHandle_t payload(void);
extern TaskHandle_t memory_manage(void);
extern void	spi_initialize(void);
extern TaskHandle_t wdt_reset(void);
extern TaskHandle_t obc_packet_router(void);
extern TaskHandle_t scheduling(void);
extern TaskHandle_t fdir(void);

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook(void);
void vApplicationIdleHook(void);
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName);
void vApplicationTickHook(void);

/************************************************************************/
/*								MAIN                                    */
/*		This is the function that is called when the reset handler		*/
/*		is triggered.													*/
/************************************************************************/

int main(void)
{
	SAFE_MODE = 0;
	safe_mode();
	
	/* Initialize Interrupt Priorities */
	prvInitializeInterruptPriorities();
	
	/* Prepare the hardware */
	prvSetupHardware();
	
	/* Initialize Global FIFOs			*/
	prvInitializeFifos();
	
	/* Initialize Global variables		*/
	prvInitializeGlobalVars();
		
	/* Create Tasks */
	//fdir_HANDLE = fdir();
	//opr_HANDLE = obc_packet_router();
	//scheduling_HANDLE = scheduling();
	//command_loop();
	//housekeeping_HANDLE = housekeep();
	//data_test();
	//time_manage_HANDLE = time_manage();
	//memory_manage_HANDLE = memory_manage();
	eps_HANDLE = eps();
	//coms_HANDLE = coms();
	//pay_HANDLE = payload();
	wdt_reset_HANDLE = wdt_reset();
	
	/* Start Scheduler */
	vTaskStartScheduler();
	
	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for tasks	to be created.*/
	
	while (1);
			
	return 0;
}
/*-----------------------------------------------------------*/

/************************************************************************/
/* SAFE_MODE															*/
/* @Purpose: Before entering into regular operation mode, the OBC waits	*/
/* for a CAN message to take it out of safe mode.						*/
/************************************************************************/
static void safe_mode(void)
{
	extern void SystemCoreClockUpdate(void);
	uint32_t timeout = 80000000, low=0, high=0;
	uint32_t MEM_LOCATION = 0x00080000;
	size_t SIZE = 10;
	/* ASF function to setup clocking. */
	sysclk_init();
	
	/* Ensure all priority bits are assigned as preemption priority bits. */
	NVIC_SetPriorityGrouping(0);
	
	/* Initializes WDT, CAN, and interrupts. */
	safe_board_init();
	
	/* Initialize Mutexes */
	prvInitializeMutexes();
	
	/* Initialize CAN-related registers and functions for tests and operation */
	can_initialize();

	low = fletcher32(MEM_LOCATION, SIZE);
	
	while(SAFE_MODE)
	{
		if(timeout--)
		{
			send_can_command(low, 0x00, OBC_ID, COMS_ID, SAFE_MODE_TYPE, COMMAND_PRIO);
			timeout = 80000000;
		}
	}
}

/************************************************************************/
/* PRVSETUPHARDWARE														*/
/* @Purpose: sets up hardware/pin values and initializes drivers		*/
/************************************************************************/
static void prvSetupHardware(void)
{
	/* Perform the remainder of board initialization functions. */
	board_init();

	/* Perform any configuration necessary to use the ParTest LED output functions. */
	vParTestInitialise();
		
	/* Initialize USART-related registers and functions. */
	usart_initialize();
	
	/* Initialize SPI related registers and functions. */
	spi_initialize();
	
	/* Initialize RTC registers and set the default initial time. */
	//rtc_init(DS3234_INTCN);
	
	/* Initialize the SPI memory chips	*/
	//spimem_initialize();
	
}
/*-----------------------------------------------------------*/

/************************************************************************/
/* PRVINITIALIZEMUTEXES													*/
/* @Purpose: initializes mutexes used for resource management			*/
/************************************************************************/
static void prvInitializeMutexes(void)
{	
	Can0_Mutex = xSemaphoreCreateBinary();
	Spi0_Mutex = xSemaphoreCreateBinary();
	Highsev_Mutex = xSemaphoreCreateBinary();
	Lowsev_Mutex = xSemaphoreCreateBinary();

	xSemaphoreGive(Can0_Mutex);
	xSemaphoreGive(Spi0_Mutex);
	xSemaphoreGive(Highsev_Mutex);
	xSemaphoreGive(Lowsev_Mutex);
	return;
}

/************************************************************************/
/* PRVINITIALIZEFIFOS													*/
/* @Purpose: initializes fifos for CAN communication and PUS services	*/
/************************************************************************/
static void prvInitializeFifos(void)
{
	UBaseType_t fifo_length, item_size;
	/* Initialize global CAN FIFOs					*/
	fifo_length = 100;		// Max number of items in the FIFO.
	item_size = 4;			// Number of bytes in the items.
		
	/* This corresponds to 400 bytes, or 50 CAN messages */
	can_data_fifo = xQueueCreate(fifo_length, item_size);
	can_msg_fifo = xQueueCreate(fifo_length, item_size);
	can_hk_fifo = xQueueCreate(fifo_length, item_size);
	can_com_fifo = xQueueCreate(fifo_length, item_size);
	tc_msg_fifo = xQueueCreate(fifo_length, item_size);
	event_msg_fifo = xQueueCreate(fifo_length, item_size);

	/* Initialize global PUS Packet FIFOs			*/
	fifo_length = 4;			// Max number of items in the FIFO.
	item_size = 147;			// Number of bytes in the items
	hk_to_obc_fifo = xQueueCreate(fifo_length, item_size);
	mem_to_obc_fifo = xQueueCreate(fifo_length, item_size);
	sched_to_obc_fifo = xQueueCreate(fifo_length, item_size);
	fdir_fifo_buffer = xQueueCreate(fifo_length, item_size);
	fifo_length = 4;
	item_size = 10;
	time_to_obc_fifo = xQueueCreate(fifo_length, item_size);

	/* Initialize global Command FIFOs				*/
	fifo_length = 4;
	item_size = 147;
	obc_to_hk_fifo = xQueueCreate(fifo_length, item_size);
	obc_to_mem_fifo = xQueueCreate(fifo_length, item_size);
	obc_to_sched_fifo = xQueueCreate(fifo_length, item_size);
	obc_to_fdir_fifo = xQueueCreate(fifo_length, item_size);
	fifo_length	 = 4;
	item_size = 10;
	obc_to_time_fifo = xQueueCreate(fifo_length, item_size);
	item_size = 152;	
	high_sev_to_fdir_fifo = xQueueCreate(fifo_length, item_size);
	low_sev_to_fdir_fifo = xQueueCreate(fifo_length, item_size);
	return;
}

/************************************************************************/
/* PRVINITIALIZEINTERRUPTPRIORITIES										*/
/* @Purpose: initializes relevant interrupt priorities					*/
/************************************************************************/
static void prvInitializeInterruptPriorities(void)
{
	uint32_t priority = 11;
	IRQn_Type can1_int_num = (IRQn_Type)44;
	IRQn_Type can0_int_num = (IRQn_Type)43;
		
	NVIC_SetPriority(can1_int_num, priority);
	
	priority = 12;	
	NVIC_SetPriority(can0_int_num, priority);
	
	priority = NVIC_GetPriority(can1_int_num);
	
	return;
}

/************************************************************************/
/* PRVINITIALIZEGLOBALVARS	                                            */
/* @Purpose: This function initializes all global variables.			*/
/************************************************************************/
static void prvInitializeGlobalVars(void)
{
	uint8_t i;
	
	/* Global Mode Variables */
	SAFE_MODE = 0;
	LOW_POWER_MODE = 0;
	COMS_TAKEOVER_MODE = 0;
	COMS_PAUSED = 0;
	PAY_PAUSED = 0;
	EPS_PAUSED = 0;
	INTERNAL_MEMORY_FALLBACK_MODE = 0;
	
	/* Initialize the data reception flag	*/
	glob_drf = 0;
		
	/* Initialize the message reception flag */
	glob_comsf = 0;
		
	/* Initialize the HK Command Flags */
	hk_read_requestedf = 0;
	hk_read_receivedf = 0;
	hk_write_requestedf = 0;
	hk_write_receivedf = 0;
		
	/* Initialize the global can regs		*/
	for (i = 0; i < 2; i++)
	{
		glob_stored_data[i] = 0;
		glob_stored_message[i] = 0;
		hk_read_receive[i] = 0;
		hk_write_receive[i] = 0;
	}
	
	for (i = 0; i < 152; i++)
	{
		high_error_array[i] = 0;
		low_error_array[i] = 0;
	}

	/* TC/ TM transaction Flags */
	tm_transfer_completef = 0;
	start_tm_transferf = 0;
	current_tc_fullf = 0;
	receiving_tcf = 0;
	
	/* FDIR signals that individuals tasks loop on */
	hk_fdir_signal = 0;
	time_fdir_signal = 0;
	coms_fdir_signal = 0;
	eps_fdir_signal = 0;
	pay_fdir_signal = 0;
	opr_fdir_signal = 0;
	sched_fdir_signal = 0;
	wdt_fdir_signal = 0;
	mem_fdir_signal = 0;
	
	/* Timeouts used for various operations */
	req_data_timeout = 2000000;		// Maximum wait time of 25ms.
	erase_sector_timeout = 30;		// Maximum wait time of 300ms.
	chip_erase_timeout = 1500;		// Maximum wait time of 15s.
	obc_consec_trans_timeout = 100;	// Maximum wait time of 100ms.
	obc_ok_go_timeout = 25;			// Maximum wait time of 25ms.
	
	/* SPI MEMORY BASE ADDRESSES	*/
	COMS_BASE		=	0x00000;	// COMS = 16kB: 0x00000 - 0x03FFF
	EPS_BASE		=	0x04000;	// EPS = 16kB: 0x04000 - 0x07FFF
	PAY_BASE		=	0x08000;	// PAY = 16kB: 0x08000 - 0x0BFFF
	HK_BASE			=	0x0C000;	// HK = 8kB: 0x0C000 - 0x0DFFF
	EVENT_BASE		=	0x0E000;	// EVENT = 8kB: 0x0E000 - 0x0FFFF
	SCHEDULE_BASE	=	0x10000;	// SCHEDULE = 8kB: 0x10000 - 0x11FFF
	SCIENCE_BASE	=	0x12000;	// SCIENCE = 8kB: 0x12000 - 0x13FFF
	TIME_BASE		=	0xFFFFC;	// TIME = 4B: 0xFFFFC - 0xFFFFF

	/* Limits for task operations */
	if(!INTERNAL_MEMORY_FALLBACK_MODE)
	{
		MAX_SCHED_COMMANDS = 511;
		LENGTH_OF_HK = 8192;
	}
	
	return;
}

void vApplicationMallocFailedHook(void)
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created.  It is also called by various parts of the
	demo application.  If heap_1.c or heap_2.c are used, then the size of the
	heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */
	taskDISABLE_INTERRUPTS();
	/* @non-terminating@ */
	for (;;);
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook(void)
{
	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
	task.  It is essential that code added to this hook function never attempts
	to block in any way (for example, call xQueueReceive() with a block time
	specified, or call vTaskDelay()).  If the application makes use of the
	vTaskDelete() API function (as this demo application does) then it is also
	important that vApplicationIdleHook() is permitted to return to its calling
	function, because it is the responsibility of the idle task to clean up
	memory allocated by the kernel to any task that has since been deleted. */
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
	(void)pcTaskName;
	(void)pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	/* @non-terminating@ */
	for (;;);
}
/*-----------------------------------------------------------*/

void vApplicationTickHook(void)
{
	/* This function will be called by each tick interrupt if
	configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
	added here, but the tick hook is called from an interrupt context, so
	code must not attempt to block, and only the interrupt safe FreeRTOS API
	functions can be used (those that end in FromISR()). */
}
/*-----------------------------------------------------------*/

