/* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Standard demo includes - just needed for the LED (ParTest) initialization
function. */
#include "partest.h"

/* Atmel library includes. */
#include <asf.h>

#include "can_func.h"

#include "semphr.h"

/* MUTEXES and SEMAPHORES */

SemaphoreHandle_t	Can1_Mutex;

/*
* Set up the hardware ready to run the program.
*/
static void prvSetupHardware(void);

/*	Initialize mutexes and semaphores to be used by the programs  */
static void prvInitializeMutexes(void);

/* External functions used to create and encapsulate different tasks*/

extern void my_blink(void);
extern void command_loop(void);
extern void housekeep(void);

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook(void);
void vApplicationIdleHook(void);
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName);
void vApplicationTickHook(void);