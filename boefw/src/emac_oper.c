#include <stdio.h>
#include <stdlib.h>
#include "xparameters.h"
#include "xil_types.h"
#include "xil_assert.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xgpiops.h"
#include "xemacps.h"
#include <sleep.h>

#define EMACPS_DEVICE_ID	XPAR_XEMACPS_0_DEVICE_ID
XEmacPs EmacPsInstance;

#define PHY_DETECT_REG1 2
#define PHY_DETECT_REG2 3

#define PHY_ID_MARVELL	0x141
#define PHY_ID_TI		0x2000
static u32 gPLPhyaddr = 0x1E;
u32 XEmacPsDetectPHY(XEmacPs * EmacPsInstancePtr)
{
	u32 PhyAddr;
	u32 Status;
	u16 PhyReg1;
	u16 PhyReg2;

	for (PhyAddr = 0; PhyAddr <= 31; PhyAddr++) {
		Status = XEmacPs_PhyRead(EmacPsInstancePtr, PhyAddr,
					  PHY_DETECT_REG1, &PhyReg1);

		Status |= XEmacPs_PhyRead(EmacPsInstancePtr, PhyAddr,
					   PHY_DETECT_REG2, &PhyReg2);

		if ((Status == XST_SUCCESS) &&
		    (PhyReg1 > 0x0000) && (PhyReg1 < 0xffff) &&
		    (PhyReg2 > 0x0000) && (PhyReg2 < 0xffff)) {
			/* Found a valid PHY address */
			XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 28, 0xb41f);
		}
	}
	return PhyAddr;		/* default to 32(max of iteration) */
}



int emac_init(void)
{
	LONG Status;
	XEmacPs_Config *Config;

	Config = XEmacPs_LookupConfig(EMACPS_DEVICE_ID);
	Status = XEmacPs_CfgInitialize(&EmacPsInstance, Config,
					Config->BaseAddress);

	if (Status != XST_SUCCESS) {
		xil_printf("xemac initialize failed.\r\n");
		return XST_FAILURE;
	}
	XEmacPsDetectPHY(&EmacPsInstance);
	return XST_SUCCESS;
}

int emac_reg_read(u32 RegisterNum, u16 *PhyDataPtr)
{
	u32 Status;
	Status = XEmacPs_PhyRead(&EmacPsInstance, gPLPhyaddr,
			RegisterNum, PhyDataPtr);
	return Status;
}

#define BCM54XX_SHD_WRITE (0x8000)
#define BCM54XX_SHD_VAL(x) ((x & 0x1f) << 10)
#define BCM54XX_SHD_DATA(x) ((x & 0x3ff) << 0)
int emac_shadow_reg_read(u32 RegisterNum, u16 shadow, u16 *PhyDataPtr)
{
	u16 regVal = 0;
	u32 Status;
	Status = XEmacPs_PhyWrite(&EmacPsInstance, gPLPhyaddr, RegisterNum, BCM54XX_SHD_VAL(shadow));
	Status |= XEmacPs_PhyRead(&EmacPsInstance, gPLPhyaddr, RegisterNum, &regVal);
	*PhyDataPtr = BCM54XX_SHD_DATA(regVal);
	return Status;
}
int emac_shadow_reg_write(u32 RegisterNum, u16 shadow, u16 val)
{
	u32 Status;
	Status = XEmacPs_PhyWrite(&EmacPsInstance, gPLPhyaddr, RegisterNum, BCM54XX_SHD_WRITE | BCM54XX_SHD_VAL(shadow) | BCM54XX_SHD_DATA(val));
	return Status;
}


int emac_reset()
{
	XGpioPs emacGpio;
	XGpioPs_Config *ConfigPtr;
	int emacResetPin = 77;

	/* Initialize the Gpio driver. */
	ConfigPtr = XGpioPs_LookupConfig(XPAR_GPIO_0_DEVICE_ID);
	if (ConfigPtr == NULL) {
		return XST_FAILURE;
	}
	XGpioPs_CfgInitialize(&emacGpio, ConfigPtr, ConfigPtr->BaseAddr);


	XGpioPs_SetDirectionPin(&emacGpio, emacResetPin, 1);
	XGpioPs_SetOutputEnablePin(&emacGpio, emacResetPin, 1);

	XGpioPs_WritePin(&emacGpio, emacResetPin, 0x0);
	usleep(30000);
	XGpioPs_WritePin(&emacGpio, emacResetPin, 0x1);

	return XST_SUCCESS;
}
