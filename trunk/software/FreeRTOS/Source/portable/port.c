/*
    FreeRTOS V8.2.0 - Copyright (C) 2015 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>!AND MODIFIED BY!<< the FreeRTOS exception.

	***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
	***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
	the FAQ page "My application does not run, what could be wrong?".  Have you
	defined configASSERT()?

	http://www.FreeRTOS.org/support - In return for receiving this top quality
	embedded software for free we request you assist our global community by
	participating in the support forum.

	http://www.FreeRTOS.org/training - Investing in training allows your team to
	be as productive as possible as early as possible.  Now you can receive
	FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
	Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*-----------------------------------------------------------
 * Implementation of functions defined in portable.h for the MicroBlaze port.
 *----------------------------------------------------------*/


/* Scheduler includes. */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

/* Standard includes. */
#include <string.h>

/* Hardware includes. */
#include "itu.h"
#include "small_printf.h"

/* Tasks are started with interrupts enabled. */
#define portINITIAL_MSR_STATE		( ( StackType_t ) 0x02 )

/* Tasks are started with a critical section nesting of 0 - however prior
to the scheduler being commenced we don't want the critical nesting level
to reach zero, so it is initialised to a high value. */
#define portINITIAL_NESTING_VALUE	( 0x00 )

/* Our hardware setup only uses one counter. */
#define portCOUNTER_0 				0

/* The stack used by the ISR is filled with a known value to assist in
debugging. */
#define portISR_STACK_FILL_VALUE	0x55555555

/* Counts the nesting depth of calls to portENTER_CRITICAL().  Each task 
maintains it's own count, so this variable is saved as part of the task
context. */
volatile UBaseType_t uxCriticalNesting = portINITIAL_NESTING_VALUE;

/* To limit the amount of stack required by each task, this port uses a
separate stack for interrupts. */
uint32_t *pulISRStack;

/*-----------------------------------------------------------*/

/*
 * Sets up the periodic ISR used for the RTOS tick.  This uses timer 0, but
 * could have alternatively used the watchdog timer or timer 1.
 */
static void prvSetupTimerInterrupt( void )
{
	ITU_IRQ_TIMER_HI = 3;
	ITU_IRQ_TIMER_LO = 208;
	ITU_IRQ_TIMER_EN = 1;
	ITU_IRQ_ENABLE = 1;
}
/*-----------------------------------------------------------*/

/* 
 * Initialise the stack of a task to look exactly as if a call to 
 * portSAVE_CONTEXT had been made.
 * 
 * See the header file portable.h.
 */
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
extern void *_SDA2_BASE_, *_SDA_BASE_;
const uint32_t ulR2 = ( uint32_t ) &_SDA2_BASE_;
const uint32_t ulR13 = ( uint32_t ) &_SDA_BASE_;

	/* Place a few bytes of known values on the bottom of the stack. 
	This is essential for the Microblaze port and these lines must
	not be omitted.  The parameter value will overwrite the 
	0x22222222 value during the function prologue. */
	*pxTopOfStack = ( StackType_t ) 0x11111111;
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x22222222;
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x33333333;
	pxTopOfStack--; 

	/* First stack an initial value for the critical section nesting.  This
	is initialised to zero as tasks are started with interrupts enabled. */
	*pxTopOfStack = ( StackType_t ) 0x00;	/* R0. */

	/* Place an initial value for all the general purpose registers. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) ulR2;	/* R2 - small data area. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x03;	/* R3. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x04;	/* R4. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) pvParameters;/* R5 contains the function call parameters. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x06;	/* R6. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x07;	/* R7. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x08;	/* R8. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x09;	/* R9. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x0a;	/* R10. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x0b;	/* R11. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x0c;	/* R12. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) ulR13;	/* R13 - small data read write area. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) pxCode;	/* R14. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x0f;	/* R15. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x10;	/* R16. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x11;	/* R17. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x12;	/* R18. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x13;	/* R19. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x14;	/* R20. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x15;	/* R21. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x16;	/* R22. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x17;	/* R23. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x18;	/* R24. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x19;	/* R25. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x1a;	/* R26. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x1b;	/* R27. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x1c;	/* R28. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x1d;	/* R29. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x1e;	/* R30. */
	pxTopOfStack--;

	/* The MSR is stacked between R30 and R31. */
	*pxTopOfStack = portINITIAL_MSR_STATE;
	pxTopOfStack--;

	*pxTopOfStack = ( StackType_t ) 0x1f;	/* R31. */
	pxTopOfStack--;

	/* Return a pointer to the top of the stack we have generated so this can
	be stored in the task control block for the task. */
	return pxTopOfStack;
}
/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler( void )
{
extern void ( __FreeRTOS_interrupt_Handler )( void );
extern void ( vStartFirstTask )( void );


	/* Setup the FreeRTOS interrupt handler.  Code copied from crt0.s. */
	__asm__ volatile ( 	"la	r6, r0, __FreeRTOS_interrupt_handler	\n\t" \
					"sw	  r6, r1, r0								\n\t" \
					"lhu  r7, r1, r0								\n\t" \
					"ori  r7, r7, 0xB0000000						\n\t" \
					"swi  r7, r0, 0x10								\n\t" \
					"andi r6, r6, 0xFFFF							\n\t" \
					"ori  r6, r6, 0xB8080000						\n\t" \
					"swi  r6, r0, 0x16 " );

	/* Setup the hardware to generate the tick.  Interrupts are disabled when
	this function is called. */
	prvSetupTimerInterrupt();

	/* Allocate the stack to be used by the interrupt handler. */
	pulISRStack = ( uint32_t * ) pvPortMalloc( configMINIMAL_STACK_SIZE * sizeof( StackType_t ) );

	/* Restore the context of the first task that is going to run. */
	if( pulISRStack != NULL )
	{
		/* Fill the ISR stack with a known value to facilitate debugging. */
		memset( pulISRStack, portISR_STACK_FILL_VALUE, configMINIMAL_STACK_SIZE * sizeof( StackType_t ) );
		pulISRStack += ( configMINIMAL_STACK_SIZE - 1 );

		microblaze_enable_interrupts();

		/* Kick off the first task. */
		vStartFirstTask();
	}

	/* Should not get here as the tasks are now running! */
	return pdFALSE;
}
/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
	/* Not implemented. */
}
/*-----------------------------------------------------------*/

/*
 * Manual context switch called by portYIELD or taskYIELD.  
 */
void vPortYield( void )
{
extern void VPortYieldASM( void );

	/* Perform the context switch in a critical section to assure it is
	not interrupted by the tick ISR.  It is not a problem to do this as
	each task maintains it's own interrupt status. */
	portENTER_CRITICAL();
		/* Jump directly to the yield function to ensure there is no
		compiler generated prologue code. */
		__asm__ volatile (	"bralid r14, VPortYieldASM		\n\t" \
						"or r0, r0, r0					\n\t" );
	portEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/*
 * The interrupt handler placed in the interrupt vector when the scheduler is
 * started.  The task context has already been saved when this is called.
 * This handler determines the interrupt source and calls the relevant 
 * peripheral handler.
 */
/*
 * Handler for the timer interrupt.
 */
void vTickISR( void )
{
	/* Increment the RTOS tick - this might cause a task to unblock. */
	xTaskIncrementTick();
	vTaskSwitchContext();

/*
	if( xTaskIncrementTick() != pdFALSE )
	{
		vTaskSwitchContext();
	}
*/
}
/*-----------------------------------------------------------*/

void vTaskISRHandler( void )
{
static uint8_t pending;

	/* Which interrupts are pending? */
	pending = ITU_IRQ_ACTIVE;
	ITU_IRQ_CLEAR = pending;

	if (pending & 0x01) {
		vTickISR();
	}
	if (pending & 0x04) {
		//usb!
	}
}
/*-----------------------------------------------------------*/
void vAssertCalled( char* fileName, uint16_t lineNo )
{
	printf("ASSERTION FAIL: %s:%d\n", fileName, lineNo);
	while(1)
		;
}




