/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * main.c: program entry.
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xplatform_info.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xparameters.h"
#include "common.h"

/*
 * Firmware main work
 * 1. upgrade
 * 2. set boeid
 * 3. set bindid
 * 4. set bindaccount
 * 5. generate random
 * 6. hwsign.
 *
 *
 * N. some function not support when not bindid or bindaccount or checkbind error.

 */
/*
 * Tmp variable.
 */
typedef enum UPGRADE_FLAG {
	UPGRADE_NONE = 0,
	UPGRADE_REBOOT = 6,
}UPGRADE_FLAG;
extern GlobalHandle gHandle;



int mainloop(void)
{
	while(gHandle.bRun){
		A_Package *pack = NULL;
		u32 timeout_ms = 2;
		if(0 != msg_pool_fetch(&gHandle.gMsgPoolIns, &pack, timeout_ms)) {//have no msg.
			// have no msg.
		}else {
			ACmd cmd = pack->header.acmd;
			switch(cmd){
			default :
				xil_printf("Get cmd %d.\n\r", cmd);
				break;
			}
		}

	}

}

int main()
{
	int status = 0;
    init_platform();
    // 1. some init.
    status = FlashInit(&gHandle.gFlashInstance, 0);
    if(status != XST_SUCCESS){
    	xil_printf("FlashInit failed.\n\r");
    	return -1;
    }
    status = env_init();
    if(status != XST_SUCCESS){
		xil_printf("Env init failed.\n\r");
		return -1;
	}
    status = env_get(&gHandle.gEnv);
    if(status != XST_SUCCESS){
		xil_printf("env get failed.\n\r");
		return -1;
	}
    status = msg_pool_init(&gHandle.gMsgPoolIns);
    if(status != XST_SUCCESS){
		xil_printf("fifo init failed.\n\r");
		return -1;
	}

    if(gHandle.gEnv.update_flag == UPGRADE_REBOOT){
		// Now is reboot from upgrade, so upgrade successful.
		// Todo response upgrade success.
    	A_Package *pack = apackage_new(200);
		gHandle.gEnv.update_flag = UPGRADE_NONE;
		env_update(&gHandle.gEnv);
		msg_pool_txsend(&gHandle.gMsgPoolIns, pack, 200);
		apackage_free(pack);
	}
    xil_printf("Welcome to HPB.\n\r");

    mainloop();



    cleanup_platform();
    return 0;
}
