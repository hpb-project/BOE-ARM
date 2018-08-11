/*
 * runtest.c
 *
 *  Created on: 2018年7月20日
 *      Author: luxq
 */

#include "env.h"
#include "flash_oper.h"
#include "led.h"

int test_led()
{
	ledInit();
	ledHigh(LED_1);
	sleep(1);
	ledHigh(LED_2);
	sleep(1);
	ledHigh(LED_3);
	sleep(1);
	ledHigh(LED_4);
	sleep(1);
	ledLow(LED_ALL);
	sleep(1);
	int i = 10;
	while(i){
		i--;
		ledHigh(LED_ALL);
		usleep(300000);
		ledLow(LED_ALL);
		usleep(300000);
	}
	return 0;
}


static u8 CmdBfr[8];

void test_flash(void)
{
	static XQspiPsu QspiPsuInstance;
    int Status;
    int byteCnt = 0x10;
    u8 defData = 0x1C;
    u32 test_addr = 0x50000;
    u32 maxdata = 0x800;
#if 0 // page test.
    u8 filldata = 0xab;
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
#if 1 // block test.
    u8 filldata = 0xC1;

    u8 wbfr[2032] = {0};
    u8 rbfr[2032] = {0};
    memset(wbfr, filldata, sizeof(wbfr));
    XQspiPsu *QspiPsuInstancePtr = &QspiPsuInstance;
    maxdata = sizeof(wbfr);
	Status = FlashInit(&QspiPsuInstance);
	if (Status != XST_SUCCESS) {
		xil_printf("Flash Init failed.\r\n");
        return ;
	}
	Status = FlashErase(&QspiPsuInstance, test_addr, 0x100000*4, CmdBfr);
	if (Status != XST_SUCCESS) {
		return ;
	}
	sleep(1);
	Status = FlashWrite(&QspiPsuInstance, test_addr, maxdata, wbfr);
	if(Status != XST_SUCCESS)
	{
		printf("Flash write failed.\n");
		return;
	}
	Status = FlashRead(QspiPsuInstancePtr, test_addr, maxdata, CmdBfr, rbfr);
	if(Status != XST_SUCCESS)
	{
		printf("Flash read failed.\n");
		return;
	}
	if(memcmp(rbfr, wbfr, maxdata) == 0) {
		printf("Flash Write and Read 256 success.\n");
	}else {
		printf("Flash Write and Read 256 failed.\n");

	}
	for(int i = 0; i < sizeof(rbfr); i++) {
		printf("read[%d]=0x%02x, write[%d]=0x%02x.\n", i, rbfr[i], i, wbfr[i]);
	}
#endif
#if 0 // test bytes.
	int i = 0;
	XQspiPsu *QspiPsuInstancePtr = &QspiPsuInstance;
	Status = FlashInit(&QspiPsuInstance, 0);
	if (Status != XST_SUCCESS) {
		xil_printf("Flash Init failed.\n");
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

void test_env(void)
{
	EnvContent env;
	int status = env_init();
	if(status != XST_SUCCESS){
		printf("env init failed.\n");
		return;
	}

	printf("env test success.\r\n");
	return;
}

int runtest()
{
	test_led();
	test_flash();
	test_env();
}

