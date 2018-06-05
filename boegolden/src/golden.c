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
 * golden.c: jump boot image.
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
#include "xil_types.h"
#include "xstatus.h"
#include "xqspipsu.h"
#include "multiboot.h"
#include "sleep.h"



void test_flash(void)
{
	static XQspiPsu QspiPsuInstance;
    int Status;
    int byteCnt = 0x10;
    u8 defData = 0x1C;
    u32 test_addr = 0x80000;
    u32 maxdata = 0x80000;
#if 0 // block test.
    u8 filldata = 0xac;
    u8 wbfr[256] = {0};
    u8 rbfr[256] = {0};
    memset(wbfr, filldata, sizeof(wbfr));
    XQspiPsu *QspiPsuInstancePtr = &QspiPsuInstance;
	Status = FlashInit(&QspiPsuInstance, 0);
	u8 CmdBfr[8] = {0};
	if (Status != XST_SUCCESS) {
		xil_printf("Flash Init failed.\r\n");
        return ;
	}
	Status = FlashErase(&QspiPsuInstance, test_addr, maxdata);
	if (Status != XST_SUCCESS) {
		return ;
	}
	Status = FlashWrite(&QspiPsuInstance, test_addr, sizeof(wbfr), wbfr);
	if(Status != XST_SUCCESS)
	{
		printf("Flash write failed.\n");
		return;
	}
	Status = FlashRead(QspiPsuInstancePtr, test_addr, sizeof(rbfr), rbfr);
	if(Status != XST_SUCCESS)
	{
		printf("Flash read failed.\n");
		return;
	}
	if(memcmp(rbfr, wbfr, sizeof(rbfr)) == 0) {
		printf("Flash Write and Read 256 success.\n");
	}else {
		printf("Flash Write and Read 256 failed.\n");
		for(int i = 0; i < sizeof(rbfr); i++) {
			printf("read[%d]=0x%02x, write[%d]=0x%02x.\n", i, rbfr[i], i, wbfr[i]);
		}
	}
#endif
#if 1 // byte test.
	int i = 0;
	XQspiPsu *QspiPsuInstancePtr = &QspiPsuInstance;
	Status = FlashInit(&QspiPsuInstance, 0);
	if (Status != XST_SUCCESS) {
		xil_printf("Flash Init failed.\r\n");
		return ;
	}
	Status = FlashErase(&QspiPsuInstance, test_addr, maxdata);
	if (Status != XST_SUCCESS) {
		return ;
	}
	for(i = 0; i < maxdata; i++)
	{
		u8 wdata = (i+1)%0xFF;
		Status = FlashWrite(&QspiPsuInstance, test_addr+i, 1, &wdata);
		if(Status != XST_SUCCESS)
		{
			printf("Flash write %d failed.\n", i);
			return;
		}
	}
	for(i = 0; i < maxdata; i++){
		u8 rdata = 0;
		u8 wdata = (i+1)%0xFF;
		Status = FlashRead(&QspiPsuInstance, test_addr + i, 1, &rdata);
		if(Status != XST_SUCCESS)
		{
			printf("Flash read %d failed.\n", i);
			return;
		}
		if(rdata != wdata){
			printf("Flash read(0x%02x) != (0x%02x).\n", rdata, wdata);
		}else {
			printf("Write and Read data [0x%02x].\n", rdata);
		}
	}
#endif
}

int main()
{
    init_platform();
#if 0
    // test code.
    print("Then will multiboot to image2, 0x40000.\n\r");
    sleep(2);

    GoMultiBoot(0x40000/0x8000);
#endif
    test_flash();

    cleanup_platform();
    return 0;
}
