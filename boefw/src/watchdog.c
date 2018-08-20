/*
 * watchdog.c
 *
 *  Created on: 2018年8月20日
 *      Author: luxq
 */


#include "xparameters.h"
#include "xwdtps.h"
#include "xil_printf.h"
#define WATCH_DOG_1MS (120)
/************************** Constant Definitions *****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are only defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define WDT_DEVICE_ID  		XPAR_XWDTPS_0_DEVICE_ID
static XWdtPs Watchdog;		/* Instance of WatchDog Timer  */


int watch_dog_init(int cnt_ms)
{
	int Status;
	XWdtPs_Config *ConfigPtr;
	u32 EffectiveAddress;	/* This can be the virtual address */

	/*
	 * Initialize the Watchdog Timer so that it is ready to use
	 */
	ConfigPtr = XWdtPs_LookupConfig(WDT_DEVICE_ID);

	/*
	 * This is where the virtual address would be used, this example
	 * uses physical address.
	 */
	EffectiveAddress = ConfigPtr->BaseAddress;
	Status = XWdtPs_CfgInitialize(&Watchdog, ConfigPtr,
					   EffectiveAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

int watch_dog_start(int cnt_ms)
{
	/*
	 * Set the initial counter restart to the smallest value (0).
	 */
	XWdtPs_SetControlValue(&Watchdog,
				(u8) XWDTPS_COUNTER_RESET, (u32) cnt_ms*WATCH_DOG_1MS);

	/*
	 * Set the initial Divider ratio at the smallest value.
	 */
	XWdtPs_SetControlValue(&Watchdog,
				(u8) XWDTPS_CLK_PRESCALE,
				(u8) XWDTPS_CCR_PSCALE_0008);

	/*
	 * Enable the RESET output.
	 */
	XWdtPs_EnableOutput(&Watchdog, XWDTPS_RESET_SIGNAL);

	/*
	 * Start the Watchdog Timer.
	 */
	XWdtPs_Start(&Watchdog);
	return XST_SUCCESS;
}

int watch_dog_restart()
{
	XWdtPs_RestartWdt(&Watchdog);
	return XST_SUCCESS;
}

int watch_dog_expired()
{
	if (XWdtPs_IsWdtExpired(&Watchdog)) {
		return 1;
	}
	return 0;
}
