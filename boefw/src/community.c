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
#include "xil_exception.h"
#include "xstreamer.h"
#include "xil_cache.h"
#include "xllfifo.h"
#include "xstatus.h"
#include "community.h"

#ifdef XPAR_INTC_0_DEVICE_ID
 #include "xintc.h"
#else
 #include "xscugic.h"
#endif

#define FIFO_DEV_ID	   	XPAR_AXI_FIFO_0_DEVICE_ID

#ifdef XPAR_INTC_0_DEVICE_ID
#define INTC_DEVICE_ID          XPAR_INTC_0_DEVICE_ID
#define FIFO_INTR_ID		XPAR_INTC_0_LLFIFO_0_VEC_ID
#else
#define INTC_DEVICE_ID          XPAR_SCUGIC_SINGLE_DEVICE_ID
#define FIFO_INTR_ID            XPAR_FABRIC_LLFIFO_0_VEC_ID
#endif

#ifdef XPAR_INTC_0_DEVICE_ID
 #define INTC           XIntc
 #define INTC_HANDLER   XIntc_InterruptHandler
#else
 #define INTC           XScuGic
 #define INTC_HANDLER   XScuGic_InterruptHandler
#endif

#define WORD_SIZE 		4
#define MSG_POOL_SPACE 1000 	// 2M
#define HANDLE_MAGIC   0x1231


typedef int (fifo_callback)(u8 *data, int len);
typedef struct F_Package{
	u8 data[PACKAGE_MAX_SIZE];
}F_Package;

typedef struct MsgPool{
	F_Package pool[MSG_POOL_SPACE];
	int w_pos;
	int r_pos;
	u16 magic;
	XLlFifo fifoIns;
	u16 bSendComplete;
}MsgPool;

static int msg_pool_full(MsgPool *mp);
static int msg_pool_empty(MsgPool *mp);
static int msg_pool_push(MsgPool *mp, F_Package *pack);
static A_Package* msg_new(int len);
static void FifoHandler(void *data);

static INTC Intc;




/****************************************************************************/
/**
*
* This function setups the interrupt system such that interrupts can occur
* for the FIFO device. This function is application specific since the
* actual system may or may not have an interrupt controller. The FIFO
* could be directly connected to a processor without an interrupt controller.
* The user should modify this function to fit the application.
*
* @param    InstancePtr contains a pointer to the instance of the FIFO
*           component which is going to be connected to the interrupt
*           controller.
*
* @return   XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note     None.
*
****************************************************************************/
int SetupInterruptSystem(INTC *IntcInstancePtr, u16 FifoIntrId, Xil_InterruptHandler iHandler, void* userdata)
{
	int Status;

#ifdef XPAR_INTC_0_DEVICE_ID
	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	Status = XIntc_Initialize(IntcInstancePtr, INTC_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


	/*
	 * Connect a device driver handler that will be called when an interrupt
	 * for the device occurs, the device driver handler performs the
	 * specific interrupt processing for the device.
	 */
	Status = XIntc_Connect(IntcInstancePtr, FifoIntrId,
			   (XInterruptHandler)FifoHandler,
			   (void *)InstancePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Start the interrupt controller such that interrupts are enabled for
	 * all devices that cause interrupts, specific real mode so that
	 * the FIFO can cause interrupts through the interrupt controller.
	 */
	Status = XIntc_Start(IntcInstancePtr, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Enable the interrupt for the AXI FIFO device.
	 */
	XIntc_Enable(IntcInstancePtr, FifoIntrId);
#else
	XScuGic_Config *IntcConfig;

	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
				IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XScuGic_SetPriorityTriggerType(IntcInstancePtr, FifoIntrId, 0xA0, 0x3);

	/*
	 * Connect the device driver handler that will be called when an
	 * interrupt for the device occurs, the handler defined above performs
	 * the specific interrupt processing for the device.
	 */
	Status = XScuGic_Connect(IntcInstancePtr, FifoIntrId,
				(Xil_InterruptHandler)iHandler,
				userdata);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	XScuGic_Enable(IntcInstancePtr, FifoIntrId);
#endif

	/*
	 * Initialize the exception table.
	 */
	Xil_ExceptionInit();

	/*
	 * Register the interrupt controller handler with the exception table.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
		(Xil_ExceptionHandler)INTC_HANDLER,
		(void *)IntcInstancePtr);;

	/*
	 * Enable exceptions.
	 */
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}
/*****************************************************************************/
/**
*
* This function disables the interrupts for the AXI FIFO device.
*
* @param	IntcInstancePtr is the pointer to the INTC component instance
* @param	FifoIntrId is interrupt ID associated for the FIFO component
*
* @return	None
*
* @note		None
*
******************************************************************************/
static void DisableIntrSystem(INTC *IntcInstancePtr, u16 FifoIntrId)
{
#ifdef XPAR_INTC_0_DEVICE_ID
	/* Disconnect the interrupts */
	XIntc_Disconnect(IntcInstancePtr, FifoIntrId);
#else
	XScuGic_Disconnect(IntcInstancePtr, FifoIntrId);
#endif
}

int FifoInit(XLlFifo *InstancePtr, u16 DeviceId, void *userdata)
{
	XLlFifo_Config *Config;
	int Status;
	int i;
	int err;
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

	/*
	 * Connect the Axi Streaming FIFO to the interrupt subsystem such
	 * that interrupts can occur. This function is application specific.
	 */
	Status = SetupInterruptSystem(&Intc, FIFO_INTR_ID, FifoHandler, userdata);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed intr setup\r\n");
		return XST_FAILURE;
	}

	XLlFifo_IntEnable(InstancePtr, XLLF_INT_ALL_MASK);
}

/*****************************************************************************/
/**
*
* TxSend routine, It will send the requested amount of data at the
* specified addr.
*
* @param	InstancePtr is a pointer to the instance of the
*		XLlFifo component.
*
* @param	SourceAddr is the address of the memory
*
* @return
*		-XST_SUCCESS to indicate success
*		-XST_FAILURE to indicate failure
*
* @note		None
*
******************************************************************************/
int FifoTxSend(XLlFifo *InstancePtr, u32  *SourceAddr, int wordlen)
{
	int i = 0;
	int txSpace = XLlFifo_iTxVacancy(InstancePtr);
	if(wordlen > txSpace)
		return XST_FAILURE;
	for(i = 0; i < wordlen; i++){
		XLlFifo_TxPutWord(InstancePtr, *(SourceAddr+(i*WORD_SIZE)));
	}

	/* Start Transmission by writing transmission length into the TLR */
	XLlFifo_iTxSetLen(InstancePtr, (wordlen * WORD_SIZE));

	/* Transmission Complete */
	return XST_SUCCESS;
}

int FifoRelease(XLlFifo *InstancePtr)
{
	DisableIntrSystem(InstancePtr, FIFO_INTR_ID);
	return XST_FAILURE;
}

/*****************************************************************************/
/**
*
* This is the Receive handler callback function.
*
* @param	InstancePtr is a reference to the Fifo device instance.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void FifoRecvHandler(XLlFifo *InstancePtr, MsgPool *mp)
{
	int i;
	u32 RxWord;
	static u32 ReceiveLength;
	static F_Package fp;

	xil_printf("Receiving Data... \n\r");

	/* Read Receive Length */
	ReceiveLength = (XLlFifo_iRxGetLen(InstancePtr))/WORD_SIZE;

	while(XLlFifo_iRxOccupancy(InstancePtr)) {
		for (i=0; i < ReceiveLength; i++) {
				RxWord = XLlFifo_RxGetWord(InstancePtr);
				*(((u32*)(fp.data))+i) = RxWord;
		}
	}

	// push into msg pool
	if(ReceiveLength > 0){
		msg_pool_push(mp, &fp);
	}

}

/*****************************************************************************/
/*
*
* This is the transfer Complete Interrupt handler function.
*
* This clears the transmit complete interrupt and set the done flag.
*
* @param	InstancePtr is a pointer to Instance of AXI FIFO device.
*
* @return	None
*
* @note		None
*
******************************************************************************/
static void FifoSendHandler(XLlFifo *InstancePtr, MsgPool *mp)
{
	XLlFifo_IntClear(InstancePtr, XLLF_INT_TC_MASK);
	mp->bSendComplete = 1; // send complete
}

/*****************************************************************************/
/**
*
* This is the Error handler callback function and this function increments the
* the error counter so that the main thread knows the number of errors.
*
* @param	InstancePtr is a pointer to Instance of AXI FIFO device.
*
* @param	Pending is a bitmask of the pending interrupts.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void FifoErrorHandler(XLlFifo *InstancePtr, u32 Pending, MsgPool *mp)
{
	if (Pending & XLLF_INT_RPURE_MASK) {
		XLlFifo_RxReset(InstancePtr);
	} else if (Pending & XLLF_INT_RPORE_MASK) {
		XLlFifo_RxReset(InstancePtr);
	} else if(Pending & XLLF_INT_RPUE_MASK) {
		XLlFifo_RxReset(InstancePtr);
	} else if (Pending & XLLF_INT_TPOE_MASK) {
		XLlFifo_TxReset(InstancePtr);
		mp->bSendComplete = 2; // send error.
	} else if (Pending & XLLF_INT_TSE_MASK) {
	}
}
/*
 * Fifo interupt handler
 */
static void FifoHandler(void *userdata)
{
	MsgPool *mp = (MsgPool*)userdata;
	XLlFifo *InstancePtr = &(mp->fifoIns);
	u32 Pending;
	Pending = XLlFifo_IntPending(InstancePtr);
	while (Pending) {
		if (Pending & XLLF_INT_RC_MASK) {
			FifoRecvHandler(InstancePtr, mp);
			XLlFifo_IntClear(InstancePtr, XLLF_INT_RC_MASK);
		}
		else if (Pending & XLLF_INT_TC_MASK) {
			FifoSendHandler(InstancePtr, mp);
		}
		else if (Pending & XLLF_INT_ERROR_MASK){
			FifoErrorHandler(InstancePtr, Pending, mp);
			XLlFifo_IntClear(InstancePtr, XLLF_INT_ERROR_MASK);
		} else {
			XLlFifo_IntClear(InstancePtr, Pending);
		}
		Pending = XLlFifo_IntPending(InstancePtr);
	}
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
	if(!msg_pool_full(mp)){
		memcpy(&(mp->pool[mp->w_pos]), pack, sizeof(F_Package));
		mp->w_pos = (mp->w_pos+1)%MSG_POOL_SPACE;
		return 0;
	}else{
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
	status = FifoInit(&(mp->fifoIns), FIFO_DEV_ID, mp);
	if(status != XST_SUCCESS){
		free(mp);
		return XST_FAILURE;
	}
	*phandle = mp;
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
		return -1;
	int pack_len = 0, bWait = 1, bGetMsg = 0;
	u32 waitus = 0, timeout_us = timeout_ms *1000;
	do{
		if(!msg_pool_empty(mp)){
			F_Package *fp = &(mp->pool[mp->w_pos]);
			A_Package *ap = (A_Package*)fp;
			pack_len = ap->header.body_length + sizeof(A_Package);
			pack = apackage_new(pack_len);
			memcpy(pack, ap, pack_len);
			mp->r_pos = (mp->r_pos+1)%MSG_POOL_SPACE;
			bGetMsg = 1;
			break;
		}else{
			if(waitus >= timeout_us)
				break;
			usleep(50);
			waitus  += 50;
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
	int status = FifoTxSend(&(mp->fifoIns), (u32*)pack, bytelen/4);
	if(status != XST_SUCCESS){
		return 1;
	}
	// wait send done.
	do{
		if(mp->bSendComplete == 1){
			return XST_SUCCESS;
		}else if(mp->bSendComplete == 2){
			return 1;
		}else if(mp->bSendComplete == 0){
			usleep(20);
			timeout_us -= 20;
		}
	}while(timeout_us > 0);
	return 2;
}
