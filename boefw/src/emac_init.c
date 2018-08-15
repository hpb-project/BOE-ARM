#include <stdio.h>
#include <stdlib.h>
#include "xparameters.h"
#include "xil_types.h"
#include "xil_assert.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xemacps.h"

#define EMACPS_DEVICE_ID	XPAR_XEMACPS_0_DEVICE_ID
XEmacPs EmacPsInstance;

#define PHY_DETECT_REG1 2
#define PHY_DETECT_REG2 3

#define PHY_ID_MARVELL	0x141
#define PHY_ID_TI		0x2000
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
