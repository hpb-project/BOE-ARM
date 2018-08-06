/*
 * atimer.c
 *
 *  Created on: 2018年7月26日
 *      Author: luxq
 */

#include <stdio.h>
#include <stdlib.h>
#include "xparameters.h"
#include "xstatus.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "xttcps.h"
#include "xscugic.h"
#include "xil_printf.h"
#include "atimer.h"

/************************** Constant Definitions *****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are only defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define TTC_TICK_DEVICE_ID	XPAR_XTTCPS_0_DEVICE_ID
#define TTC_TICK_INTR_ID	XPAR_XTTCPS_0_INTR
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID

/*
 * Constants to set the basic operating parameters.
 * PWM_DELTA_DUTY is critical to the running time of the test. Smaller values
 * make the test run longer.
 */
#define	TICK_TIMER_FREQ_HZ	100000  /* Tick timer counter's output frequency */

// 1ms per cycle
#define TICKS_PER_CHANGE_PERIOD	100 /* Tick signals per update */

/**************************** Type Definitions *******************************/
typedef struct {
	u32 OutputHz;	/* Output frequency */
	XInterval Interval;	/* Interval value */
	u8 Prescaler;	/* Prescaler value */
	u16 Options;	/* Option settings */
} TmrCntrSetup;

typedef struct {
	TimerFunc func;
	u64 internal_ms;
	u64 tickcount;
	void *userdata;
	u8    bUsed;
}TmrRegisterInfo;

static int TmrRtcInterruptExample(void);  /* Main test */

/* Set up routines for timer counters */
static int SetupTicker(void);
static int SetupTimer(int DeviceID);

/* Interleaved interrupt test for both timer counters */
static int WaitForDutyCycleFull(void);

static int SetupInterruptSystem(u16 IntcDeviceID, XScuGic *IntcInstancePtr);

static void TickHandler(void *CallBackRef);

/************************** Variable Definitions *****************************/

static XTtcPs TtcPsInst;  /* Timer counter instance */

static TmrCntrSetup SettingsTable=
	{TICK_TIMER_FREQ_HZ, 0, 0, 0};	/* Ticker timer counter initial setup,
						only output freq */

static XScuGic InterruptController;  	/* Interrupt controller instance */

static u8 ErrorCount;		/* Errors seen at interrupt time */
static u32 TickCount;		/* Ticker interrupts between seconds change */
static volatile u64 msCount;
static TmrRegisterInfo gRegisterInfo[MAX_TIMER_FUNC_NUM];

int atimer_init(void)
{
	int Status;

	/*
	 * Make sure the interrupts are disabled, in case this is being run
	 * again after a failure.
	 */

	/*
	 * Connect the Intc to the interrupt subsystem such that interrupts can
	 * occur.  This function is application specific.
	 */
	Status = SetupInterruptSystem(INTC_DEVICE_ID, &InterruptController);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Set up  the Ticker timer
	 */
	Status = SetupTicker();
	if (Status != XST_SUCCESS) {
		return Status;
	}
	return XST_SUCCESS;
}

int atimer_release()
{
	/*
	 * Stop the counters
	 */
	XTtcPs_Stop(&TtcPsInst);
	return XST_SUCCESS;
}

/****************************************************************************/
/**
*
* This function sets up the Ticker timer.
*
* @param	None
*
* @return	XST_SUCCESS if everything sets up well, XST_FAILURE otherwise.
*
*****************************************************************************/
int SetupTicker(void)
{
	int Status;
	TmrCntrSetup *TimerSetup;
	XTtcPs *TtcPsTick;

	TimerSetup = &SettingsTable;

	/*
	 * Set up appropriate options for Ticker: interval mode without
	 * waveform output.
	 */
	TimerSetup->Options |= (XTTCPS_OPTION_INTERVAL_MODE |
					      XTTCPS_OPTION_WAVE_DISABLE);

	/*
	 * Calling the timer setup routine
	 *  . initialize device
	 *  . set options
	 */
	Status = SetupTimer(TTC_TICK_DEVICE_ID);
	if(Status != XST_SUCCESS) {
		return Status;
	}

	TtcPsTick = &TtcPsInst;

	/*
	 * Connect to the interrupt controller
	 */
	Status = XScuGic_Connect(&InterruptController, TTC_TICK_INTR_ID,
		(Xil_InterruptHandler)TickHandler, (void *)TtcPsTick);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Enable the interrupt for the Timer counter
	 */
	XScuGic_Enable(&InterruptController, TTC_TICK_INTR_ID);

	/*
	 * Enable the interrupts for the tick timer/counter
	 * We only care about the interval timeout.
	 */
	XTtcPs_EnableInterrupts(TtcPsTick, XTTCPS_IXR_INTERVAL_MASK);

	/*
	 * Start the tick timer/counter
	 */
	XTtcPs_Start(TtcPsTick);

	return Status;
}


/****************************************************************************/
/**
*
* This function sets up a timer counter device, using the information in its
* setup structure.
*  . initialize device
*  . set options
*  . set interval and prescaler value for given output frequency.
*
* @param	DeviceID is the unique ID for the device.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
*****************************************************************************/
int SetupTimer(int DeviceID)
{
	int Status;
	XTtcPs_Config *Config;
	XTtcPs *Timer;
	TmrCntrSetup *TimerSetup;

	TimerSetup = &SettingsTable;

	Timer = &TtcPsInst;

	/*
	 * Look up the configuration based on the device identifier
	 */
	Config = XTtcPs_LookupConfig(DeviceID);
	if (NULL == Config) {
		return XST_FAILURE;
	}

	/*
	 * Initialize the device
	 */
	Status = XTtcPs_CfgInitialize(Timer, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Set the options
	 */
	XTtcPs_SetOptions(Timer, TimerSetup->Options);

	/*
	 * Timer frequency is preset in the TimerSetup structure,
	 * however, the value is not reflected in its other fields, such as
	 * IntervalValue and PrescalerValue. The following call will map the
	 * frequency to the interval and prescaler values.
	 */
	XTtcPs_CalcIntervalFromFreq(Timer, TimerSetup->OutputHz,
		&(TimerSetup->Interval), &(TimerSetup->Prescaler));

	/*
	 * Set the interval and prescale
	 */
	XTtcPs_SetInterval(Timer, TimerSetup->Interval);
	XTtcPs_SetPrescaler(Timer, TimerSetup->Prescaler);

	return XST_SUCCESS;
}

/****************************************************************************/
/**
*
* This function setups the interrupt system such that interrupts can occur.
* This function is application specific since the actual system may or may not
* have an interrupt controller.  The TTC could be directly connected to a
* processor without an interrupt controller.  The user should modify this
* function to fit the application.
*
* @param	IntcDeviceID is the unique ID of the interrupt controller
* @param	IntcInstacePtr is a pointer to the interrupt controller
*		instance.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
*****************************************************************************/
static int SetupInterruptSystem(u16 IntcDeviceID,
				    XScuGic *IntcInstancePtr)
{
	int Status;
	XScuGic_Config *IntcConfig; /* The configuration parameters of the
					   interrupt controller */

	/*
	 * Initialize the interrupt controller driver
	 */
	IntcConfig = XScuGic_LookupConfig(IntcDeviceID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


	/*
	 * Connect the interrupt controller interrupt handler to the hardware
	 * interrupt handling logic in the ARM processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
				(Xil_ExceptionHandler) XScuGic_InterruptHandler,
				IntcInstancePtr);


	/*
	 * Enable interrupts in the ARM
	 */
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

static void refresh_register_function()
{
	int i = 0;
	for(i = 0; i < MAX_TIMER_FUNC_NUM; i++){
		TmrRegisterInfo *info = &(gRegisterInfo[i]);
		if(info->bUsed){
			info->tickcount++;
			if(info->tickcount >= info->internal_ms){
				int ret = info->func(info->userdata);
				if(ret == 0){
					info->bUsed = 0;
				}else{
					info->tickcount = 0;
				}
			}
		}
	}
}
/***************************************************************************/
/**
*
* This function is the handler which handles the periodic tick interrupt.
* It updates its count, and set a flag to signal PWM timer counter to
* update its duty cycle.
*
* This handler provides an example of how to handle data for the TTC and
* is application specific.
*
* @param	CallBackRef contains a callback reference from the driver, in
*		this case it is the instance pointer for the TTC driver.
*
* @return	None.
*
*****************************************************************************/
static void TickHandler(void *CallBackRef)
{
	u32 StatusEvent;

	/*
	 * Read the interrupt status, then write it back to clear the interrupt.
	 */
	StatusEvent = XTtcPs_GetInterruptStatus((XTtcPs *)CallBackRef);
	XTtcPs_ClearInterruptStatus((XTtcPs *)CallBackRef, StatusEvent);

	if (0 != (XTTCPS_IXR_INTERVAL_MASK & StatusEvent)) {
		TickCount++;

		if (TICKS_PER_CHANGE_PERIOD == TickCount) {
			TickCount = 0;
			msCount++;
			refresh_register_function();
		}

	}
	else {
		/*
		 * The Interval event should be the only one enabled. If it is
		 * not it is an error
		 */
		ErrorCount++;
	}
}

void atimer_reset()
{
	msCount = 0;
}

u64 atimer_gettm()
{
	return msCount;
}

int atimer_register_timer(TimerFunc func, u64 internal_ms, void *userdata)
{
	int i = 0;
	if(func == NULL || internal_ms == 0){
		return -1;
	}
	for(i = 0; i < MAX_TIMER_FUNC_NUM; i++)
	{
		if(gRegisterInfo[i].bUsed == 0){
			gRegisterInfo[i].func = func;
			gRegisterInfo[i].internal_ms = internal_ms;
			gRegisterInfo[i].userdata = userdata;
			gRegisterInfo[i].tickcount = 0;
			gRegisterInfo[i].bUsed = 1;
			break;
		}
	}
	if(i >= MAX_TIMER_FUNC_NUM){
		return -2;
	}
	return 0;
}
