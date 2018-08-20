/*
 * runtest.c
 *
 *  Created on: Jun 6, 2018
 *      Author: luxq
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

#include "flash_oper.h"
#include "flash_map.h"
#include "env.h"
#include "emac_oper.h"
#include "led.h"
static u8 CmdBfr[8];

void test_flash(void)
{
	static XQspiPsu QspiPsuInstance;
    int Status;
    int byteCnt = 0x10;
    u8 defData = 0x1C;
    u32 test_addr = 0x50000;
    u32 maxdata = 0x800;

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

}

void test_env(void)
{
	EnvContent env;
	int status = env_init();
	if(status != XST_SUCCESS){
		printf("env init failed.\n");
		return;
	}
	for(int i = 0 ; i < 1000; i++){
		status = env_get(&env);
		if(status != XST_SUCCESS){
			printf("%d, env get failed\n", i);
			return;
		}
		if(i != 0){
			printf("get reversed_3 = %d.\n", env.reversed_3);
			if(env.reversed_3 != i){
				printf("get reversed_3 failed, id=%d,should = %d.\n", env.reversed_3, i);
				return;
			}
		}
		env.reversed_3 = i+1;
		status = env_update(&env);
		if(status != XST_SUCCESS){
			printf("%d, env update failed\n", i);
			return;
		}
	}
	printf("env test success.\r\n");
	return;
}

void test_led()
{
	ledInit();
	while(1){
		ledHigh(LED_1);
		sleep(2);
		ledHigh(LED_1 | LED_2);
		sleep(2);
		ledHigh(LED_1 | LED_2 | LED_3);
		sleep(2);
		ledHigh(LED_1 | LED_2 | LED_3 | LED_4);
		sleep(2);
		int slit = 10;
		while(slit-- > 0){
			ledLow(LED_ALL);
			usleep(200000);
			ledHigh(LED_ALL);
			usleep(200000);
		}
		break;
	}
}

void test_emac()
{

	u16 val = 0;
	emac_init();

	emac_reg_read(0x2, &val);
	xil_printf("phy [0x%02x] = 0x%02x\r\n", 0x02, val);
	emac_reg_read(0x3, &val);
	xil_printf("phy [0x%02x] = 0x%02x\r\n", 0x03, val);
	while(1){
		sleep(1);
		emac_shadow_reg_read(0x1c, 0x1F, &val);
		xil_printf("phy [0x%02x], shadow = 0x%02x, val = 0x%02x\r\n", 0x1c, 0x1f, val);
	}
}

extern void runtest(void)
{
    //test_flash();
    //at508_test();
    //test_led();
    //test_env();
	test_emac();

}

