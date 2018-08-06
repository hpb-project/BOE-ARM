/***************************** Include Files *********************************/
//#define DUMMY_LED
#ifndef DUMMY_LED
#include "xparameters.h"
#include "xgpio.h"
#include "xil_printf.h"
#include "led.h"

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define GPIO_EXAMPLE_DEVICE_ID  XPAR_GPIO_0_DEVICE_ID

/*
 * The following constant is used to determine which channel of the GPIO is
 * used for the LED if there are 2 channels supported.
 */
#define LED_CHANNEL 1

/*
 * The following are declared globally so they are zeroed and so they are
 * easily accessible from a debugger
 */

XGpio Gpio; /* The Instance of the GPIO Driver */

int ledInit(void)
{
	int Status;

	/* Initialize the GPIO driver */
	Status = XGpio_Initialize(&Gpio, GPIO_EXAMPLE_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Gpio Initialization Failed\r\n");
		return XST_FAILURE;
	}

	/* Set the direction for all signals as inputs except the LED output */
	XGpio_SetDataDirection(&Gpio, LED_CHANNEL, ~LED_ALL);
	return XST_SUCCESS;
}

int ledHigh(u8 led)
{
	u8 iled = led & 0x0f; // clear high 4 bit
	XGpio_DiscreteWrite(&Gpio, LED_CHANNEL, iled);
	return XST_SUCCESS;
}

int ledLow(u8 led)
{
	u8 iled = led & 0x0f;
	XGpio_DiscreteClear(&Gpio, LED_CHANNEL, iled);
	return XST_SUCCESS;
}
#else

#include "xparameters.h"
#include "xil_printf.h"
#include "xstatus.h"
#include "led.h"


/*
 * The following constant is used to determine which channel of the GPIO is
 * used for the LED if there are 2 channels supported.
 */
#define LED_CHANNEL 1

int ledInit(void)
{
	return XST_SUCCESS;
}

int ledHigh(u8 led)
{
	return XST_SUCCESS;
}

int ledLow(u8 led)
{
	return XST_SUCCESS;
}
#endif
