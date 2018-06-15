/*
 * multiboot.c
 *
 *  Created on: Jun 4, 2018
 *      Author: luxq
 */

#include <stdio.h>
#include "xplatform_info.h"
#include "xil_io.h"
#include "xil_types.h"
#include "multiboot.h"


#define CSU_BASEADDR      		0XFFCA0000U
#define CSU_CSU_MULTI_BOOT    	( ( CSU_BASEADDR ) + 0X00000010U )

#define CRL_APB_BASEADDR      	0XFF5E0000U
/**
 * Register: CRL_APB_RPLL_CTRL
 */
#define CRL_APB_RPLL_CTRL    	( ( CRL_APB_BASEADDR ) + 0X00000030U )
#define CRL_APB_RPLL_CTRL_BYPASS_MASK    0X00000008U
/**
 * Register: CRL_APB_RESET_CTRL
 */
#define CRL_APB_RESET_CTRL    ( ( CRL_APB_BASEADDR ) + 0X00000218U )
#define CRL_APB_RESET_CTRL_SOFT_RESET_MASK    0X00000010U


void GoReset(void)
{
	u32 RegValue;
	/**
	 * Due to a bug in 1.0 Silicon, PS hangs after System Reset if RPLL is used.
	 * Hence, just for 1.0 Silicon, bypass the RPLL clock before giving
	 * System Reset.
	 */
	if (XGetPSVersion_Info() == (u32)XPS_VERSION_1)
	{
		RegValue = Xil_In32(CRL_APB_RPLL_CTRL) |
				CRL_APB_RPLL_CTRL_BYPASS_MASK;
		Xil_Out32(CRL_APB_RPLL_CTRL, RegValue);
	}

	/* make sure every thing completes */
	dsb();
	isb();
	/* Soft reset the system */
	printf("Multiboot Performing System Soft Reset\n\r");
	RegValue = Xil_In32(CRL_APB_RESET_CTRL);
	Xil_Out32(CRL_APB_RESET_CTRL,
			RegValue|CRL_APB_RESET_CTRL_SOFT_RESET_MASK);

	/* wait here until reset happens */
	while(1) {
		;
	}
}

void GoMultiBoot(u32 MultiBootValue)
{

	Xil_Out32(CSU_CSU_MULTI_BOOT, MultiBootValue);
	GoReset();

	return;

}

