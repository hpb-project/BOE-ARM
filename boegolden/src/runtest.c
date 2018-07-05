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


void test_flash(void)
{
	static XQspiPsu QspiPsuInstance;
    int Status;
    int byteCnt = 0x10;
    u8 defData = 0x1C;
    u32 test_addr = 0x60000;
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
    u8 filldata = 0xab;
    u8 wbfr[2050] = {0};
    u8 rbfr[2050] = {0};
    memset(wbfr, filldata, sizeof(wbfr));
    XQspiPsu *QspiPsuInstancePtr = &QspiPsuInstance;
    maxdata = sizeof(wbfr);
	Status = FlashInit(&QspiPsuInstance);
	if (Status != XST_SUCCESS) {
		xil_printf("Flash Init failed.\r\n");
        return ;
	}
	Status = FlashErase(&QspiPsuInstance, test_addr, 0x1000*4);
	if (Status != XST_SUCCESS) {
		return ;
	}
	Status = FlashWrite(&QspiPsuInstance, test_addr, maxdata, wbfr);
	if(Status != XST_SUCCESS)
	{
		printf("Flash write failed.\n");
		return;
	}
	Status = FlashRead(QspiPsuInstancePtr, test_addr, maxdata, rbfr);
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
	for(int i = 0 ; i < 1000; i++){
		status = env_get(&env);
		if(status != XST_SUCCESS){
			printf("%d, env get failed\n", i);
			return;
		}
		if(i != 0){
			printf("get boeid = %d.\n", env.boeid);
			if(env.boeid != i){
				printf("get boeid failed, id=%d,should = %d.\n", env.boeid, i);
				return;
			}
		}
		env.boeid = i+1;
		status = env_update(&env);
		if(status != XST_SUCCESS){
			printf("%d, env update failed\n", i);
			return;
		}
	}
	printf("env test success.\r\n");
	return;
}

extern void runtest(void)
{
    // test multiboot
    //GoMultiBoot(0x40000/0x8000);
    //test_flash();
    test_env();
}

