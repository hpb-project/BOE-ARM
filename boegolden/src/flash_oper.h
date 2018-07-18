/*
 * flash_oper.h : provide flash read/write function.
 *
 *  Created on: Jun 4, 2018
 *      Author: luxq
 */

#ifndef SRC_FLASH_OPER_H_
#define SRC_FLASH_OPER_H_

#include "xqspipsu.h"

/**************************** Type Definitions *******************************/
#include "xparameters.h"	/* SDK generated parameters */
#include "xqspipsu.h"		/* QSPIPSU device driver */
#include "xil_printf.h"
#include "xil_cache.h"

typedef struct{
	u32 SectSize;		/* Individual sector size or
						 * combined sector size in case of parallel config*/
	u32 NumSect;		/* Total no. of sectors in one/two flash devices */
	u32 PageSize;		/* Individual page size or
						 * combined page size in case of parallel config*/
	u32 NumPage;		/* Total no. of pages in one/two flash devices */
	u32 FlashDeviceSize;	/* This is the size of one flash device
						 * NOT the combination of both devices, if present
						 */
	u8 ManufacturerID;	/* Manufacturer ID - used to identify make */
	u8 DeviceIDMemSize;	/* Byte of device ID indicating the memory size */
	u32 SectMask;		/* Mask to get sector start address */
	u8 NumDie;			/* No. of die forming a single flash */
}FlashInfo;


int FlashInit(XQspiPsu *QspiPsuInstancePtr);
int FlashReadID(XQspiPsu *QspiPsuPtr);
int FlashGetInfo(FlashInfo *fi);
int FlashErase(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount, u8 *WriteBfrPtr);

int FlashWrite(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount, u8 *WriteBfrPtr);

int FlashRead(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount,
				u8 *WriteBfrPtr, u8 *ReadBfrPtr);
u32 GetRealAddr(XQspiPsu *QspiPsuPtr, u32 Address);
int BulkErase(XQspiPsu *QspiPsuPtr, u8 *WriteBfrPtr);
int DieErase(XQspiPsu *QspiPsuPtr, u8 *WriteBfrPtr);
int FlashRegisterRead(XQspiPsu *QspiPsuPtr, u32 ByteCount, u8 Command, u8 *ReadBfrPtr);
int FlashRegisterWrite(XQspiPsu *QspiPsuPtr, u32 ByteCount, u8 Command,
					u8 *WriteBfrPtr, u8 WrEn);
int FlashEnterExit4BAddMode(XQspiPsu *QspiPsuPtr,unsigned int Enable);
#endif /* SRC_FLASH_OPER_H_ */
