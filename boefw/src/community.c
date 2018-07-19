// Copyright 2018 The go-hpb Authors
// This file is part of the go-hpb.
//
// The go-hpb is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// The go-hpb is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with the go-hpb. If not, see <http://www.gnu.org/licenses/>.


#include <stdio.h>
#include <string.h>
#include <time.h>
#include "xil_types.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "xstreamer.h"
#include "xil_cache.h"
#include "xllfifo.h"
#include "xstatus.h"
#include "community.h"

#define FIFO_DEV_ID	   	XPAR_AXI_FIFO_0_DEVICE_ID
#define WORD_SIZE 		4
#define MSG_POOL_SPACE 1000 	// 1M
#define HANDLE_MAGIC   0x1231


typedef int (fifo_callback)(u8 *data, int len);
typedef struct F_Package{
	u8 data[PACKAGE_MAX_SIZE];
}F_Package;

typedef struct MsgPool{
	int w_pos;
	int r_pos;
	u16 magic;
	u16 bSendComplete;
	XLlFifo fifoIns;
	F_Package pool[MSG_POOL_SPACE];
}MsgPool;

static int msg_pool_full(MsgPool *mp);
static int msg_pool_empty(MsgPool *mp);
static int msg_pool_push(MsgPool *mp, F_Package *pack);
static A_Package* msg_new(int len);


int FifoInit(XLlFifo *InstancePtr, u16 DeviceId)
{
	XLlFifo_Config *Config;
	int Status;
	Status = XST_SUCCESS;

	/* Initialize the Device Configuration Interface driver */
	Config = XLlFfio_LookupConfig(DeviceId);
	if (!Config) {
		xil_printf("No config found for %d\r\n", DeviceId);
		return XST_FAILURE;
	}

	/*
	 * This is where the virtual address would be used, this example
	 * uses physical address.
	 */
	Status = XLlFifo_CfgInitialize(InstancePtr, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("Initialization failed\n\r");
		return Status;
	}

	/* Check for the Reset value */
	Status = XLlFifo_Status(InstancePtr);
	XLlFifo_IntClear(InstancePtr,0xffffffff);
	Status = XLlFifo_Status(InstancePtr);
	if(Status != 0x0) {
		xil_printf("\n ERROR : Reset value of ISR0 : 0x%x\t"
				"Expected : 0x0\n\r",
				XLlFifo_Status(InstancePtr));
		return XST_FAILURE;
	}
	return XST_SUCCESS;
}

int FifoSend(XLlFifo *InstancePtr, u8  *SourceAddr, int sendlen)
{
	int i;
	u32 *wBuf;
	u32 wlen;
	int timeout = 2000000; // 2sec
	if(sendlen%WORD_SIZE == 0){
		wlen = sendlen/WORD_SIZE;
	}else{
		wlen = sendlen/WORD_SIZE + 1;
	}
	wBuf = (u32*)malloc(WORD_SIZE * wlen);
	memset(wBuf, 0, WORD_SIZE * wlen);
	memcpy((u8*)wBuf, SourceAddr, sendlen);

	for(i=0 ; i < wlen ; i++){
		/* Writing into the FIFO Transmit Port Buffer */
		if( XLlFifo_iTxVacancy(InstancePtr) ){
			XLlFifo_TxPutWord(InstancePtr,
				*(wBuf+i));
		}
	}
	free(wBuf);

	/* Start Transmission by writing transmission length into the TLR */
	XLlFifo_iTxSetLen(InstancePtr, (wlen*WORD_SIZE));

	/* Check for Transmission completion */
	while( !(XLlFifo_IsTxDone(InstancePtr)) ){
		usleep(1000);
		timeout -= 1000;
		if(timeout < 0)
		{
			return 1;
		}
	}

	/* Transmission Complete */
	return XST_SUCCESS;
}

int FifoRelease(XLlFifo *InstancePtr)
{
	return XST_FAILURE;
}

int FifoReceive (XLlFifo *InstancePtr, MsgPool *mp)
{
	int i;
	u32 RxWord;
	u32 wlen = 0;
	static F_Package fp;
	u32 *dest = (u32*)(fp.data);
	int status;

	//xil_printf(" Receiving data ....\n\r");
	/* Read Recieve Length */
	while(XLlFifo_iRxOccupancy(InstancePtr) > 0){
		memset(&fp, 0x0, sizeof(fp));
		wlen  = (XLlFifo_iRxGetLen(InstancePtr));
        if(wlen%WORD_SIZE == 0)
        {
            wlen = wlen/WORD_SIZE;
        }
        else
        {
            wlen = wlen/WORD_SIZE + 1;
        }
#if 0
        if((wlen*WORD_SIZE) > sizeof(F_Package))
        {
            xil_printf("package size(%d) > F_Package size(%d).\r\n", wlen*WORD_SIZE, sizeof(F_Package));
            while(1);
        }
#endif
		/* Start Receiving */
		for ( i=0; i < wlen; i++){
			RxWord = XLlFifo_RxGetWord(InstancePtr);
			*(dest+i) = RxWord;
		}
		//xil_printf("fifo get a package.\r\n");
		// push into msg pool
		if(wlen > 0){
			status = msg_pool_push(mp, &fp);
			if(status != 0)
				xil_printf("push failed.\r\n");
		}
	}

	return XST_SUCCESS;
}


/*
 * return value: 0: not full
 * 				 1: is full.
 */
static int msg_pool_full(MsgPool *mp)
{
	if(1 == ((mp->r_pos + MSG_POOL_SPACE - mp->w_pos)%MSG_POOL_SPACE)){
		return 1;
	}else {
		return 0;
	}
}

/*
 * return value: 0: not empty,
 * 				 1: empty
 */
static int msg_pool_empty(MsgPool *mp)
{
	if(mp->r_pos == mp->w_pos)
		return 1;
	else
		return 0;
}

/*
 * return value: 0: push success
 * 				 1: failed, pool is full.
 */
static int msg_pool_push(MsgPool *mp, F_Package *pack)
{
	int idx;
	if(!msg_pool_full(mp)){
		idx = mp->w_pos;
        //xil_printf("push package to idx = %d.\r\n", idx);
		memset(&(mp->pool[idx]), 0x0, sizeof(F_Package));
        //if(mp->w_pos == 999 || mp->w_pos == 0)
        //{
        //    for(int j = 0; j < sizeof(F_Package); j++)
        //    {
        //        xil_printf("p[%d]=0x%02x\r\n", j, pack->data[j]);
        //    }
        //}
		memcpy(&(mp->pool[idx]), pack, sizeof(F_Package));
		mp->w_pos = (mp->w_pos+1)%MSG_POOL_SPACE;
		return 0;
	}else{
		xil_printf("msg pool is full.\r\n");
		return 1;
	}
}


int msg_pool_init(MsgPoolHandle *phandle)
{
	int status = 0;
	MsgPool *mp = (MsgPool*)malloc(sizeof(MsgPool));
	mp->w_pos = 0;
	mp->r_pos = 0;
	mp->magic = HANDLE_MAGIC;
	memset(mp->pool, 0, sizeof(mp->pool));
	status = FifoInit(&(mp->fifoIns), FIFO_DEV_ID);
	if(status != XST_SUCCESS){
		free(mp);
		return XST_FAILURE;
	}
	*phandle = mp;
	xil_printf("msg_pool_init success.\r\n");
	return XST_SUCCESS;
}

int msg_pool_release(MsgPoolHandle *phandle)
{
	MsgPool *mp = NULL;
	if(!phandle && !(*phandle))
	{
		mp = *phandle;
		if(mp->magic != HANDLE_MAGIC){
			// not a msg pool handle.
			return -1;
		}
		// release fifo.
		FifoRelease(&(mp->fifoIns));
		free(mp);
		*phandle = NULL;
		return 0;
	}
	else
	{
		return -2;
	}
}

/*
 * return value: 0: get msg
 * 				 1: not get msg.
 */
int msg_pool_fetch(MsgPoolHandle handle, A_Package **pack, u32 timeout_ms)
{
	MsgPool *mp = (MsgPool*)handle;
	if(mp->magic != HANDLE_MAGIC)
	{
		xil_printf("mp-> maigc != HANDLE_MAGIC.\r\n");
		return -1;
	}
	int pack_len = 0, bWait = 1, bGetMsg = 0;
	u32 waitus = 0, timeout_us = timeout_ms *1000;
	do{
		FifoReceive(&mp->fifoIns, mp);
		if(!msg_pool_empty(mp)){
			F_Package *fp = &(mp->pool[mp->r_pos]);
            //xil_printf("r_pos = %d, w_pos = %d.\r\n", mp->r_pos, mp->w_pos);
			*pack = (A_Package*)fp;
			mp->r_pos = (mp->r_pos+1)%MSG_POOL_SPACE;
			bGetMsg = 1;
			break;
		}else{
			xil_printf("msg_pool is empty.\r\n");
			if(waitus >= timeout_us)
				break;
			usleep(1000);
			waitus  += 1000;
		}
	}while(bWait);
	return bGetMsg ? 0 : 1;
}


/*
 * return value: 0: send success.
 * 				 1: send error.
 * 				 2: send timeout.
 * 				-1: handle invalid.
 */
int msg_pool_txsend(MsgPoolHandle handle, A_Package *pack, int bytelen)
{
	MsgPool *mp = (MsgPool*)handle;
	if(mp->magic != HANDLE_MAGIC)
		return -1;
	u32 timeout_us = 20*1000; // 20ms
	int status = FifoSend(&(mp->fifoIns), (u8*)pack, bytelen);
	if(status != XST_SUCCESS){
		return 1;
	}
	return XST_SUCCESS;
}
