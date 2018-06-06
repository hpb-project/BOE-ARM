
/******************************************************************************
*
* Copyright (C) 2014 Xilinx, Inc. All rights reserved.
*
* This file contains confidential and proprietary information  of Xilinx, Inc.
* and is protected under U.S. and  international copyright and other
* intellectual property  laws.
*
* DISCLAIMER
* This disclaimer is not a license and does not grant any  rights to the
* materials distributed herewith. Except as  otherwise provided in a valid
* license issued to you by  Xilinx, and to the maximum extent permitted by
* applicable law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND  WITH ALL
* FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES  AND CONDITIONS, EXPRESS,
* IMPLIED, OR STATUTORY, INCLUDING  BUT NOT LIMITED TO WARRANTIES OF
* MERCHANTABILITY, NON-  INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE;
* and
* (2) Xilinx shall not be liable (whether in contract or tort,  including
* negligence, or under any other theory of liability) for any loss or damage
* of any kind or nature  related to, arising under or in connection with these
* materials, including for any direct, or any indirect,  special, incidental,
* or consequential loss or damage  (including loss of data, profits,
* goodwill, or any type of  loss or damage suffered as a result of any
* action brought  by a third party) even if such damage or loss was
* reasonably foreseeable or Xilinx had been advised of the  possibility
* of the same.
*
* CRITICAL APPLICATIONS
* Xilinx products are not designed or intended to be fail-  safe, or for use
* in any application requiring fail-safe  performance, such as life-support
* or safety devices or  systems, Class III medical devices, nuclear
* facilities,  applications related to the deployment of airbags, or any
* other applications that could lead to death, personal  injury, or severe
* property or environmental damage  (individually and collectively,
* "Critical  Applications"). Customer assumes the sole risk and  liability
* of any use of Xilinx products in Critical  Applications, subject only to
* applicable laws and  regulations governing limitations on product liability.
*
* THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS  PART
* OF THIS FILE AT ALL TIMES.
*
******************************************************************************/

/***************************** Include Files *********************************/

#include "xparameters.h"	/* SDK generated parameters */
#include "xqspipsu.h"		/* QSPIPSU device driver */
#include "xil_printf.h"
#include "xil_cache.h"
#include "flash_oper.h"

/************************** Constant Definitions *****************************/

/*
 * The following constants define the commands which may be sent to the Flash
 * device.
 */
#define WRITE_STATUS_CMD	0x01
#define WRITE_CMD			0x02			// PAGE PROGRAM
#define READ_CMD			0x03
#define WRITE_DISABLE_CMD	0x04
#define READ_STATUS_CMD		0x05
#define WRITE_ENABLE_CMD	0x06
#define FAST_READ_CMD		0x0B
#define DUAL_READ_CMD		0x3B
#define QUAD_READ_CMD		0x6B
#define BULK_ERASE_CMD		0xC7
#define	SEC_ERASE_CMD		0xD8
#define READ_ID				0x9F
#define READ_CONFIG_CMD		0x35
#define WRITE_CONFIG_CMD	0x01
#define ENTER_4B_ADDR_MODE	0xB7
#define EXIT_4B_ADDR_MODE	0xE9
#define EXIT_4B_ADDR_MODE_ISSI	0x29

#define WRITE_CMD_4B		0x12
#define READ_CMD_4B		0x13
#define FAST_READ_CMD_4B	0x0C
#define DUAL_READ_CMD_4B		0x3C
#define QUAD_READ_CMD_4B	0x6C
#define	SEC_ERASE_CMD_4B	0xDC

#define BANK_REG_RD			0x16
#define BANK_REG_WR			0x17
/* Bank register is called Extended Address Register in Micron */
#define EXTADD_REG_RD		0xC8
#define EXTADD_REG_WR		0xC5
#define	DIE_ERASE_CMD		0xC4
#define READ_FLAG_STATUS_CMD	0x70

/*
 * The following constants define the offsets within a FlashBuffer data
 * type for each kind of data.  Note that the read data offset is not the
 * same as the write data because the QSPIPSU driver is designed to allow full
 * duplex transfers such that the number of bytes received is the number
 * sent and received.
 */
#define COMMAND_OFFSET		0 /* Flash instruction */
#define ADDRESS_1_OFFSET	1 /* MSB byte of address to read or write */
#define ADDRESS_2_OFFSET	2 /* Middle byte of address to read or write */
#define ADDRESS_3_OFFSET	3 /* LSB byte of address to read or write */
#define ADDRESS_4_OFFSET	4 /* LSB byte of address to read or write when 4 byte address */
#define DATA_OFFSET		5 /* Start of Data for Read/Write */
#define DUMMY_OFFSET		4 /* Dummy byte offset for fast, dual and quad
				     reads */
#define DUMMY_SIZE		1 /* Number of dummy bytes for fast, dual and
				     quad reads */
#define DUMMY_CLOCKS		8 /* Number of dummy bytes for fast, dual and
				     quad reads */
#define RD_ID_SIZE		4 /* Read ID command + 3 bytes ID response */
#define BULK_ERASE_SIZE		1 /* Bulk Erase command size */
#define SEC_ERASE_SIZE		4 /* Sector Erase command + Sector address */
#define BANK_SEL_SIZE	2 /* BRWR or EARWR command + 1 byte bank value */
#define RD_CFG_SIZE		2 /* 1 byte Configuration register + RD CFG command*/
#define WR_CFG_SIZE		3 /* WRR command + 1 byte each Status and Config Reg*/
#define DIE_ERASE_SIZE	4	/* Die Erase command + Die address */

/*
 * The following constants specify the extra bytes which are sent to the
 * Flash on the QSPIPSu interface, that are not data, but control information
 * which includes the command and address
 */
#define OVERHEAD_SIZE		4

/*
 * Base address of Flash1
 */
#define FLASH1BASE 0x0000000

/*
 * Sixteen MB
 */
#define SIXTEENMB 0x1000000

/*
 * Command buffer size
 */
#define CMD_BFR_SIZE 8


/*
 * Mask for quad enable bit in Flash configuration register
 */
#define FLASH_QUAD_EN_MASK 0x02

#define FLASH_SRWD_MASK 0x80

/*
 * Bank mask
 */
#define BANKMASK 0xF000000

/*
 * Identification of Flash
 * Micron:
 * Byte 0 is Manufacturer ID;
 * Byte 1 is first byte of Device ID - 0xBB or 0xBA
 * Byte 2 is second byte of Device ID describes flash size:
 * 128Mbit : 0x18; 256Mbit : 0x19; 512Mbit : 0x20
 * Spansion:
 * Byte 0 is Manufacturer ID;
 * Byte 1 is Device ID - Memory Interface type - 0x20 or 0x02
 * Byte 2 is second byte of Device ID describes flash size:
 * 128Mbit : 0x18; 256Mbit : 0x19; 512Mbit : 0x20
 */
#define MICRON_ID_BYTE0		0x20
#define MICRON_ID_BYTE2_128	0x18
#define MICRON_ID_BYTE2_256	0x19
#define MICRON_ID_BYTE2_512	0x20
#define MICRON_ID_BYTE2_1G	0x21

#define SPANSION_ID_BYTE0		0x01
#define SPANSION_ID_BYTE2_128	0x18
#define SPANSION_ID_BYTE2_256	0x19
#define SPANSION_ID_BYTE2_512	0x20

#define WINBOND_ID_BYTE0		0xEF
#define WINBOND_ID_BYTE2_128	0x18

#define MACRONIX_ID_BYTE0		0xC2
#define MACRONIX_ID_BYTE2_1G	0x1B

#define ISSI_ID_BYTE0			0x9D
#define ISSI_ID_BYTE2_256		0x19

/*
 * The index for Flash config table
 */
/* Spansion*/
#define SPANSION_INDEX_START			0
#define FLASH_CFG_TBL_SINGLE_128_SP		SPANSION_INDEX_START
#define FLASH_CFG_TBL_STACKED_128_SP	(SPANSION_INDEX_START + 1)
#define FLASH_CFG_TBL_PARALLEL_128_SP	(SPANSION_INDEX_START + 2)
#define FLASH_CFG_TBL_SINGLE_256_SP		(SPANSION_INDEX_START + 3)
#define FLASH_CFG_TBL_STACKED_256_SP	(SPANSION_INDEX_START + 4)
#define FLASH_CFG_TBL_PARALLEL_256_SP	(SPANSION_INDEX_START + 5)
#define FLASH_CFG_TBL_SINGLE_512_SP		(SPANSION_INDEX_START + 6)
#define FLASH_CFG_TBL_STACKED_512_SP	(SPANSION_INDEX_START + 7)
#define FLASH_CFG_TBL_PARALLEL_512_SP	(SPANSION_INDEX_START + 8)

/* Micron */
#define MICRON_INDEX_START				(FLASH_CFG_TBL_PARALLEL_512_SP + 1)
#define FLASH_CFG_TBL_SINGLE_128_MC		MICRON_INDEX_START
#define FLASH_CFG_TBL_STACKED_128_MC	(MICRON_INDEX_START + 1)
#define FLASH_CFG_TBL_PARALLEL_128_MC	(MICRON_INDEX_START + 2)
#define FLASH_CFG_TBL_SINGLE_256_MC		(MICRON_INDEX_START + 3)
#define FLASH_CFG_TBL_STACKED_256_MC	(MICRON_INDEX_START + 4)
#define FLASH_CFG_TBL_PARALLEL_256_MC	(MICRON_INDEX_START + 5)
#define FLASH_CFG_TBL_SINGLE_512_MC		(MICRON_INDEX_START + 6)
#define FLASH_CFG_TBL_STACKED_512_MC	(MICRON_INDEX_START + 7)
#define FLASH_CFG_TBL_PARALLEL_512_MC	(MICRON_INDEX_START + 8)
#define FLASH_CFG_TBL_SINGLE_1GB_MC		(MICRON_INDEX_START + 9)
#define FLASH_CFG_TBL_STACKED_1GB_MC	(MICRON_INDEX_START + 10)
#define FLASH_CFG_TBL_PARALLEL_1GB_MC	(MICRON_INDEX_START + 11)

/* Winbond */
#define WINBOND_INDEX_START				(FLASH_CFG_TBL_PARALLEL_1GB_MC + 1)
#define FLASH_CFG_TBL_SINGLE_128_WB		WINBOND_INDEX_START
#define FLASH_CFG_TBL_STACKED_128_WB	(WINBOND_INDEX_START + 1)
#define FLASH_CFG_TBL_PARALLEL_128_WB	(WINBOND_INDEX_START + 2)

/* Macronix */
#define MACRONIX_INDEX_START			(FLASH_CFG_TBL_PARALLEL_128_WB + 1)
#define FLASH_CFG_TBL_SINGLE_1G_MX		MACRONIX_INDEX_START
#define FLASH_CFG_TBL_STACKED_1G_MX		(MACRONIX_INDEX_START + 1)
#define FLASH_CFG_TBL_PARALLEL_1G_MX	(MACRONIX_INDEX_START + 2)

/* ISSI */
#define ISSI_INDEX_START				(FLASH_CFG_TBL_PARALLEL_1G_MX + 1)
#define FLASH_CFG_TBL_SINGLE_256_ISSI	ISSI_INDEX_START
#define FLASH_CFG_TBL_STACKED_256_ISSI	(ISSI_INDEX_START + 1)
#define FLASH_CFG_TBL_PARALLEL_256_ISSI	(ISSI_INDEX_START + 2)

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define QSPIPSU_DEVICE_ID		XPAR_XQSPIPSU_0_DEVICE_ID

#define ENTER_4B	1
#define EXIT_4B		0

/************************** Variable Definitions *****************************/
static u8 gReadCmd;
static u8 gWriteCmd;
static u8 gStatusCmd;
static u8 gSectorEraseCmd;
static u8 gFSRFlag;
static FlashInfo gFlash_Config_Table[28] = {
		/* Spansion */
		{0x10000, 0x100, 256, 0x10000, 0x1000000,
				SPANSION_ID_BYTE0, SPANSION_ID_BYTE2_128, 0xFFFF0000, 1},
		{0x10000, 0x200, 256, 0x20000, 0x1000000,
				SPANSION_ID_BYTE0, SPANSION_ID_BYTE2_128, 0xFFFF0000, 1},
		{0x20000, 0x100, 512, 0x10000, 0x1000000,
				SPANSION_ID_BYTE0, SPANSION_ID_BYTE2_128, 0xFFFE0000, 1},
		{0x10000, 0x200, 256, 0x20000, 0x2000000,
				SPANSION_ID_BYTE0, SPANSION_ID_BYTE2_256, 0xFFFF0000, 1},
		{0x10000, 0x400, 256, 0x40000, 0x2000000,
				SPANSION_ID_BYTE0, SPANSION_ID_BYTE2_256, 0xFFFF0000, 1},
		{0x20000, 0x200, 512, 0x20000, 0x2000000,
				SPANSION_ID_BYTE0, SPANSION_ID_BYTE2_256, 0xFFFE0000, 1},
		{0x40000, 0x100, 512, 0x20000, 0x4000000,
				SPANSION_ID_BYTE0, SPANSION_ID_BYTE2_512, 0xFFFC0000, 1},
		{0x40000, 0x200, 512, 0x40000, 0x4000000,
				SPANSION_ID_BYTE0, SPANSION_ID_BYTE2_512, 0xFFFC0000, 1},
		{0x80000, 0x100, 1024, 0x20000, 0x4000000,
				SPANSION_ID_BYTE0, SPANSION_ID_BYTE2_512, 0xFFF80000, 1},
		/* Spansion 1Gbit is handled as 512Mbit stacked */
		/* Micron */
		{0x10000, 0x100, 256, 0x10000, 0x1000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_128, 0xFFFF0000, 1},
		{0x10000, 0x200, 256, 0x20000, 0x1000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_128, 0xFFFF0000, 1},
		{0x20000, 0x100, 512, 0x10000, 0x1000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_128, 0xFFFE0000, 1},
		{0x10000, 0x200, 256, 0x20000, 0x2000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_256, 0xFFFF0000, 1},
		{0x10000, 0x400, 256, 0x40000, 0x2000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_256, 0xFFFF0000, 1},
		{0x20000, 0x200, 512, 0x20000, 0x2000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_256, 0xFFFE0000, 1},
		{0x10000, 0x400, 256, 0x40000, 0x4000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_512, 0xFFFF0000, 2},
		{0x10000, 0x800, 256, 0x80000, 0x4000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_512, 0xFFFF0000, 2},
		{0x20000, 0x400, 512, 0x40000, 0x4000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_512, 0xFFFE0000, 2},
		{0x10000, 0x800, 256, 0x80000, 0x8000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_1G, 0xFFFF0000, 4},
		{0x10000, 0x1000, 256, 0x100000, 0x8000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_1G, 0xFFFF0000, 4},
		{0x20000, 0x800, 512, 0x80000, 0x8000000,
				MICRON_ID_BYTE0, MICRON_ID_BYTE2_1G, 0xFFFE0000, 4},
		/* Winbond */
		{0x10000, 0x100, 256, 0x10000, 0x1000000,
				WINBOND_ID_BYTE0, WINBOND_ID_BYTE2_128, 0xFFFF0000, 1},
		{0x10000, 0x200, 256, 0x20000, 0x1000000,
				WINBOND_ID_BYTE0, WINBOND_ID_BYTE2_128, 0xFFFF0000, 1},
		{0x20000, 0x100, 512, 0x10000, 0x1000000,
				WINBOND_ID_BYTE0, WINBOND_ID_BYTE2_128, 0xFFFE0000, 1},
		/* Macronix */
		{0x10000, 0x800, 256, 0x80000, 0x8000000,
				MACRONIX_ID_BYTE0, MACRONIX_ID_BYTE2_1G, 0xFFFF0000, 4},
		{0x10000, 0x1000, 256, 0x100000, 0x8000000,
				MACRONIX_ID_BYTE0, MACRONIX_ID_BYTE2_1G, 0xFFFF0000, 4},
		{0x20000, 0x800, 512, 0x80000, 0x8000000,
				MACRONIX_ID_BYTE0, MACRONIX_ID_BYTE2_1G, 0xFFFE0000, 4},
		/* ISSI */
		{0x10000, 0x200, 256, 0x20000, 0x2000000,
				ISSI_ID_BYTE0, ISSI_ID_BYTE2_256, 0xFFFF0000, 1}
};

static u32 gFlashMake;
static u32 gFCTIndex;	/* Flash configuration table index */
static u8  gInit = 0;
static int FlashWriteInpage(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount, u8 *WriteBfrPtr);

/*
 * The instances to support the device drivers are global such that they
 * are initialized to zero each time the program runs. They could be local
 * but should at least be static so they are zeroed.
 */

static XQspiPsu_Msg gFlashMsg[5];


/***************************** Inner function declare ***********************************/
u32 GetRealAddr(XQspiPsu *QspiPsuPtr, u32 Address);
int BulkErase(XQspiPsu *QspiPsuPtr, u8 *WriteBfrPtr);
int DieErase(XQspiPsu *QspiPsuPtr, u8 *WriteBfrPtr);
int FlashReadID(XQspiPsu *QspiPsuPtr, u8 *ReadBfrPtr);
int FlashEnterExit4BAddMode(XQspiPsu *QspiPsuPtr,unsigned int Enable);


/*****************************************************************************/
/**
*
* The purpose of this function is to illustrate how to use the XQspiPsu
* device driver in single, parallel and stacked modes using
* flash devices greater than or equal to 128Mb.
* This function reads data in DMA mode.
*
* @param	None.
*
* @return	XST_SUCCESS if successful, else XST_FAILURE.
*
* @note		None.
*
*****************************************************************************/
int FlashInit(XQspiPsu *QspiPsuInstancePtr, u16 QspiPsuDeviceId)
{
	XQspiPsu_Config *QspiPsuConfig;
	int Status;

	/*
	 * Initialize the QSPIPSU driver so that it's ready to use
	 */
	QspiPsuConfig = XQspiPsu_LookupConfig(QspiPsuDeviceId);
	if (NULL == QspiPsuConfig) {
		return XST_FAILURE;
	}

	/* To test, change connection mode here if not obtained from HDF */
	//QspiPsuConfig->ConnectionMode = 2;

	Status = XQspiPsu_CfgInitialize(QspiPsuInstancePtr, QspiPsuConfig,
					QspiPsuConfig->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Set Manual Start
	 */
	XQspiPsu_SetOptions(QspiPsuInstancePtr, XQSPIPSU_MANUAL_START_OPTION);

	/*
	 * Set the prescaler for QSPIPSU clock
	 */
	XQspiPsu_SetClkPrescaler(QspiPsuInstancePtr, XQSPIPSU_CLK_PRESCALE_8);

	XQspiPsu_SelectFlash(QspiPsuInstancePtr,
		XQSPIPSU_SELECT_FLASH_CS_LOWER, XQSPIPSU_SELECT_FLASH_BUS_LOWER);

	/*
	 * Read flash ID and obtain all flash related information
	 * It is important to call the read id function before
	 * performing proceeding to any operation, including
	 * preparing the WriteBuffer
	 */
	u8 readidbuf[3] = {0};
	Status = FlashReadID(QspiPsuInstancePtr, readidbuf);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	xil_printf("Flash connection mode : %d \n\r",
			QspiPsuConfig->ConnectionMode);
	xil_printf("where 0 - Single; 1 - Stacked; 2 - Parallel \n\r");
	xil_printf("FCTIndex: %d \n\r", gFCTIndex);

	/*
	 * Address size and read command selection
	 * Micron flash on REMUS doesn't support this 4B write/erase cmd
	 */
	gReadCmd = READ_CMD; //QUAD_READ_CMD;
	gWriteCmd = WRITE_CMD;
	gSectorEraseCmd = SEC_ERASE_CMD;

	/* Status cmd - SR or FSR selection */
	if((gFlash_Config_Table[gFCTIndex].NumDie > 1) &&
			(gFlashMake == MICRON_ID_BYTE0)) {
		gStatusCmd = READ_FLAG_STATUS_CMD;
		gFSRFlag = 1;
	} else {
		gStatusCmd = READ_STATUS_CMD;
		gFSRFlag = 0;
	}

	xil_printf("ReadCmd: 0x%x, WriteCmd: 0x%x, gStatusCmd: 0x%x, gFSRFlag: %d \n\r",
				gReadCmd, gWriteCmd, gStatusCmd, gFSRFlag);

	if(gFlash_Config_Table[gFCTIndex].FlashDeviceSize > SIXTEENMB) {
		Status = FlashEnterExit4BAddMode(QspiPsuInstancePtr,ENTER_4B);
		if(Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		xil_printf("Flash Enter 4B addr mode.\r\n");
	}
	return XST_SUCCESS;
}

int FlashRelease(XQspiPsu *QspiPsuPtr)
{
    int Status;
	if(gFlash_Config_Table[gFCTIndex].FlashDeviceSize > SIXTEENMB) {
		Status = FlashEnterExit4BAddMode(QspiPsuPtr,EXIT_4B);
		if(Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
	}
	return XST_SUCCESS;
}

int GetFlashInfo(XQspiPsu *QspiPsuPtr, FlashInfo *info)
{
	if (gFCTIndex < sizeof(gFlash_Config_Table)/sizeof(FlashInfo))
	{
		memcpy(info, &(gFlash_Config_Table[gFCTIndex]), sizeof(FlashInfo));
		return XST_SUCCESS;
	}
	return XST_FAILURE;
}

/*****************************************************************************/
/**
*
* Reads the flash ID and identifies the flash in FCT table.
*
* @param	ReadBfrPtr 3bytes array, save flashid.
*
* @return	XST_SUCCESS if successful, else XST_FAILURE.
*
* @note		None.
*
*****************************************************************************/
int FlashReadID(XQspiPsu *QspiPsuPtr, u8 *ReadBfrPtr)
{
	int Status;
	int StartIndex;
    u8 ReadIdCmd;

	/*
	 * Read ID
	 */
	ReadIdCmd = READ_ID;
	gFlashMsg[0].TxBfrPtr = &ReadIdCmd;
	gFlashMsg[0].RxBfrPtr = NULL;
	gFlashMsg[0].ByteCount = 1;
	gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

	gFlashMsg[1].TxBfrPtr = NULL;
	gFlashMsg[1].RxBfrPtr = ReadBfrPtr;
	gFlashMsg[1].ByteCount = 3;
	gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;

	Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 2);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	xil_printf("FlashID=0x%x 0x%x 0x%x\n\r", ReadBfrPtr[0], ReadBfrPtr[1],
		   ReadBfrPtr[2]);

	/* In case of dual, read both and ensure they are same make/size */

	/*
	 * Deduce flash make
	 */
	if(ReadBfrPtr[0] == MICRON_ID_BYTE0) {
		gFlashMake = MICRON_ID_BYTE0;
		StartIndex = MICRON_INDEX_START;
	}else if(ReadBfrPtr[0] == SPANSION_ID_BYTE0) {
		gFlashMake = SPANSION_ID_BYTE0;
		StartIndex = SPANSION_INDEX_START;
	}else if(ReadBfrPtr[0] == WINBOND_ID_BYTE0) {
		gFlashMake = WINBOND_ID_BYTE0;
		StartIndex = WINBOND_INDEX_START;
	} else if(ReadBfrPtr[0] == MACRONIX_ID_BYTE0) {
		gFlashMake = MACRONIX_ID_BYTE0;
		StartIndex = MACRONIX_INDEX_START;
	} else if(ReadBfrPtr[0] == ISSI_ID_BYTE0) {
		gFlashMake = ISSI_ID_BYTE0;
		StartIndex = ISSI_INDEX_START;
	}



	/*
	 * If valid flash ID, then check connection mode & size and
	 * assign corresponding index in the Flash configuration table
	 */
	if(((gFlashMake == MICRON_ID_BYTE0) || (gFlashMake == SPANSION_ID_BYTE0)||
			(gFlashMake == WINBOND_ID_BYTE0)) &&
			(ReadBfrPtr[2] == MICRON_ID_BYTE2_128)) {

		switch(QspiPsuPtr->Config.ConnectionMode)
		{
			case XQSPIPSU_CONNECTION_MODE_SINGLE:
				gFCTIndex = FLASH_CFG_TBL_SINGLE_128_SP + StartIndex;
				break;
			case XQSPIPSU_CONNECTION_MODE_PARALLEL:
				gFCTIndex = FLASH_CFG_TBL_PARALLEL_128_SP + StartIndex;
				break;
			case XQSPIPSU_CONNECTION_MODE_STACKED:
				gFCTIndex = FLASH_CFG_TBL_STACKED_128_SP + StartIndex;
				break;
			default:
				gFCTIndex = 0;
				break;
		}
	}
	/* 256 and 512Mbit supported only for Micron and Spansion, not Winbond */
	if(((gFlashMake == MICRON_ID_BYTE0) || (gFlashMake == SPANSION_ID_BYTE0)) &&
			(ReadBfrPtr[2] == MICRON_ID_BYTE2_256)) {

		switch(QspiPsuPtr->Config.ConnectionMode)
		{
			case XQSPIPSU_CONNECTION_MODE_SINGLE:
				gFCTIndex = FLASH_CFG_TBL_SINGLE_256_SP + StartIndex;
				break;
			case XQSPIPSU_CONNECTION_MODE_PARALLEL:
				gFCTIndex = FLASH_CFG_TBL_PARALLEL_256_SP + StartIndex;
				break;
			case XQSPIPSU_CONNECTION_MODE_STACKED:
				gFCTIndex = FLASH_CFG_TBL_STACKED_256_SP + StartIndex;
				break;
			default:
				gFCTIndex = 0;
				break;
		}
	}
	if((gFlashMake == ISSI_ID_BYTE0) &&
			(ReadBfrPtr[2] == MICRON_ID_BYTE2_256)) {
		switch(QspiPsuPtr->Config.ConnectionMode)
		{
			case XQSPIPSU_CONNECTION_MODE_SINGLE:
				gFCTIndex = FLASH_CFG_TBL_SINGLE_256_ISSI;
				break;
			case XQSPIPSU_CONNECTION_MODE_PARALLEL:
				gFCTIndex = FLASH_CFG_TBL_PARALLEL_256_ISSI;
				break;
			case XQSPIPSU_CONNECTION_MODE_STACKED:
				gFCTIndex = FLASH_CFG_TBL_STACKED_256_ISSI;
				break;
			default:
				gFCTIndex = 0;
				break;
		}
	}
	if(((gFlashMake == MICRON_ID_BYTE0) || (gFlashMake == SPANSION_ID_BYTE0)) &&
			(ReadBfrPtr[2] == MICRON_ID_BYTE2_512)) {

		switch(QspiPsuPtr->Config.ConnectionMode)
		{
			case XQSPIPSU_CONNECTION_MODE_SINGLE:
				gFCTIndex = FLASH_CFG_TBL_SINGLE_512_SP + StartIndex;
				break;
			case XQSPIPSU_CONNECTION_MODE_PARALLEL:
				gFCTIndex = FLASH_CFG_TBL_PARALLEL_512_SP + StartIndex;
				break;
			case XQSPIPSU_CONNECTION_MODE_STACKED:
				gFCTIndex = FLASH_CFG_TBL_STACKED_512_SP + StartIndex;
				break;
			default:
				gFCTIndex = 0;
				break;
		}
	}
	/*
	 * 1Gbit Single connection supported for Spansion.
	 * The ConnectionMode will indicate stacked as this part has 2 SS
	 * The device ID will indicate 512Mbit.
	 * This configuration is handled as the above 512Mbit stacked configuration
	 */
	/* 1Gbit single, parallel and stacked supported for Micron */
	if((gFlashMake == MICRON_ID_BYTE0) &&
			(ReadBfrPtr[2] == MICRON_ID_BYTE2_1G)) {

		switch(QspiPsuPtr->Config.ConnectionMode)
		{
			case XQSPIPSU_CONNECTION_MODE_SINGLE:
				gFCTIndex = FLASH_CFG_TBL_SINGLE_1GB_MC;
				break;
			case XQSPIPSU_CONNECTION_MODE_PARALLEL:
				gFCTIndex = FLASH_CFG_TBL_PARALLEL_1GB_MC;
				break;
			case XQSPIPSU_CONNECTION_MODE_STACKED:
				gFCTIndex = FLASH_CFG_TBL_STACKED_1GB_MC;
				break;
			default:
				gFCTIndex = 0;
				break;
		}
	}

	/* 1Gbit single, parallel and stacked supported for Macronix */
	if(((gFlashMake == MACRONIX_ID_BYTE0) &&
			(ReadBfrPtr[2] == MACRONIX_ID_BYTE2_1G))) {

		switch(QspiPsuPtr->Config.ConnectionMode) {
			case XQSPIPSU_CONNECTION_MODE_SINGLE:
				gFCTIndex = FLASH_CFG_TBL_SINGLE_1G_MX;
				break;
			case XQSPIPSU_CONNECTION_MODE_PARALLEL:
				gFCTIndex = FLASH_CFG_TBL_PARALLEL_1G_MX;
				break;
			case XQSPIPSU_CONNECTION_MODE_STACKED:
				gFCTIndex = FLASH_CFG_TBL_STACKED_1G_MX;
				break;
			default:
				gFCTIndex = 0;
				break;
		}
	}

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function writes to the  serial Flash connected to the QSPIPSU interface.
* All the data put into the buffer must be in the same page of the device with
* page boundaries being on 256 byte boundaries.
*
* @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
* @param	Address contains the address to write data to in the Flash.
* @param	ByteCount contains the number of bytes to write.
* @param	Pointer to the write buffer (which is to be transmitted)
*
* @return	XST_SUCCESS if successful, else XST_FAILURE.
*
* @note		None.
*
******************************************************************************/
static int FlashWriteInpage(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount, u8 *WriteBfrPtr)
{
	u8 WriteEnableCmd;
	u8 ReadStatusCmd;
	u8 FlashStatus[2];
	u8 WriteCmd[CMD_BFR_SIZE] = {0};
	u32 RealAddr;
	u32 CmdByteCount;
	int Status;
	u8 Command = gWriteCmd;

	WriteEnableCmd = WRITE_ENABLE_CMD;
	/*
	 * Translate address based on type of connection
	 * If stacked assert the slave select based on address
	 */
	RealAddr = GetRealAddr(QspiPsuPtr, Address);

	/*
	 * Send the write enable command to the Flash so that it can be
	 * written to, this needs to be sent as a separate transfer before
	 * the write
	 */
	gFlashMsg[0].TxBfrPtr = &WriteEnableCmd;
	gFlashMsg[0].RxBfrPtr = NULL;
	gFlashMsg[0].ByteCount = 1;
	gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

	Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	WriteCmd[COMMAND_OFFSET]   = Command;

	/* To be used only if 4B address program cmd is supported by flash */
	if(gFlash_Config_Table[gFCTIndex].FlashDeviceSize > SIXTEENMB) {
		WriteCmd[ADDRESS_1_OFFSET] = (u8)((RealAddr & 0xFF000000) >> 24);
		WriteCmd[ADDRESS_2_OFFSET] = (u8)((RealAddr & 0xFF0000) >> 16);
		WriteCmd[ADDRESS_3_OFFSET] = (u8)((RealAddr & 0xFF00) >> 8);
		WriteCmd[ADDRESS_4_OFFSET] = (u8)(RealAddr & 0xFF);
		CmdByteCount = 5;
	} else {
		WriteCmd[ADDRESS_1_OFFSET] = (u8)((RealAddr & 0xFF0000) >> 16);
		WriteCmd[ADDRESS_2_OFFSET] = (u8)((RealAddr & 0xFF00) >> 8);
		WriteCmd[ADDRESS_3_OFFSET] = (u8)(RealAddr & 0xFF);
		CmdByteCount = 4;
	}

	gFlashMsg[0].TxBfrPtr = WriteCmd;
	gFlashMsg[0].RxBfrPtr = NULL;
	gFlashMsg[0].ByteCount = CmdByteCount;
	gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

	gFlashMsg[1].TxBfrPtr = WriteBfrPtr;
	gFlashMsg[1].RxBfrPtr = NULL;
	gFlashMsg[1].ByteCount = ByteCount;
	gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_TX;
	if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
		gFlashMsg[1].Flags |= XQSPIPSU_MSG_FLAG_STRIPE;
	}

	Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 2);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Wait for the write command to the Flash to be completed, it takes
	 * some time for the data to be written
	 */
	while (1) {
		ReadStatusCmd = gStatusCmd;
		gFlashMsg[0].TxBfrPtr = &ReadStatusCmd;
		gFlashMsg[0].RxBfrPtr = NULL;
		gFlashMsg[0].ByteCount = 1;
		gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		gFlashMsg[1].TxBfrPtr = NULL;
		gFlashMsg[1].RxBfrPtr = FlashStatus;
		gFlashMsg[1].ByteCount = 2;
		gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;
		if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
			gFlashMsg[1].Flags |= XQSPIPSU_MSG_FLAG_STRIPE;
		}

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 2);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
			if(gFSRFlag) {
				FlashStatus[1] &= FlashStatus[0];
			} else {
				FlashStatus[1] |= FlashStatus[0];
			}
		}

		if(gFSRFlag) {
			if ((FlashStatus[1] & 0x80) != 0) {
				break;
			}
		} else {
			if ((FlashStatus[1] & 0x01) == 0) {
				break;
			}
		}
	}

	return 0;
}

/*****************************************************************************/
/**
*
* This function writes to the  serial Flash connected to the QSPIPSU interface.
* All the data put into the buffer not be limited in the same page of the device,
*
* @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
* @param	Address contains the address to write data to in the Flash.
* @param	ByteCount contains the number of bytes to write.
* @param	Pointer to the write buffer (which is to be transmitted)
*
* @return	XST_SUCCESS if successful, else XST_FAILURE.
*
* @note		None.
*
******************************************************************************/
int FlashWrite(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount, u8 *WriteBfrPtr)
{
	u32 page_els = 0; // page empty bytes.
	u32 page_size = (gFlash_Config_Table[gFCTIndex]).PageSize;
	u32 write_len = 0;
	int Status;
#if 0 // overflow check.
	if((Address + ByteCount) > (gFlash_Config_Table[gFCTIndex]).FlashDeviceSize)
	{
		xil_printf("Overflow the flash size.\n");
		return XST_FAILURE;
	}
#endif
	while(ByteCount > 0)
	{
		page_els = page_size - Address%page_size;
		write_len = ByteCount>page_els ? page_els : ByteCount;    	//选择较小的作为本次写入的数据字节长度
		//printf("Write %d bytes to 0x%08x, wait to write %dbytes.\n", write_len, Address, ByteCount - write_len);
		Status = FlashWriteInpage(QspiPsuPtr,Address,write_len, WriteBfrPtr);
		//printf("write finished.\n");
		if(Status != XST_SUCCESS)
		{
			break;
		}
		//改变相应参数
		ByteCount = ByteCount - write_len;            						//减少写入数量
		WriteBfrPtr = WriteBfrPtr + write_len;              				//增加写入缓冲地址
		Address = Address + write_len;                              		//增加FLASH写入的地址
	}
	return Status;
}

/*****************************************************************************/
/**
*
* This function erases the sectors in the  serial Flash connected to the
* QSPIPSU interface.
*
* @param	QspiPtr is a pointer to the QSPIPSU driver component to use.
* @param	Address contains the address of the first sector which needs to
*		be erased.
* @param	ByteCount contains the total size to be erased.
* @param	Pointer to the write buffer (which is to be transmitted)
*
* @return	XST_SUCCESS if successful, else XST_FAILURE.
*
* @note		None.
*
******************************************************************************/
int FlashErase(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount)
{
	u8 WriteEnableCmd;
	u8 ReadStatusCmd;
	u8 FlashStatus[2];
	int Sector;
	u32 RealAddr;
	u32 NumSect;
	int Status;
	u8 CmdBfr[CMD_BFR_SIZE] = {0};

	WriteEnableCmd = WRITE_ENABLE_CMD;
	/*
	 * If erase size is same as the total size of the flash, use bulk erase
	 * command or die erase command multiple times as required
	 */
	if (ByteCount == ((gFlash_Config_Table[gFCTIndex]).NumSect *
			(gFlash_Config_Table[gFCTIndex]).SectSize) ) {

		if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_STACKED){
			XQspiPsu_SelectFlash(QspiPsuPtr,
				XQSPIPSU_SELECT_FLASH_CS_LOWER, XQSPIPSU_SELECT_FLASH_BUS_LOWER);
		}

		if(gFlash_Config_Table[gFCTIndex].NumDie == 1) {
			/*
			 * Call Bulk erase
			 */
			BulkErase(QspiPsuPtr, CmdBfr);
		}

		if(gFlash_Config_Table[gFCTIndex].NumDie > 1) {
			/*
			 * Call Die erase
			 */
			DieErase(QspiPsuPtr, CmdBfr);
		}
		/*
		 * If stacked mode, bulk erase second flash
		 */
		if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_STACKED){

			XQspiPsu_SelectFlash(QspiPsuPtr,
				XQSPIPSU_SELECT_FLASH_CS_UPPER, XQSPIPSU_SELECT_FLASH_BUS_LOWER);

			if(gFlash_Config_Table[gFCTIndex].NumDie == 1) {
				/*
				 * Call Bulk erase
				 */
				BulkErase(QspiPsuPtr, CmdBfr);
			}

			if(gFlash_Config_Table[gFCTIndex].NumDie > 1) {
				/*
				 * Call Die erase
				 */
				DieErase(QspiPsuPtr, CmdBfr);
			}
		}

		return 0;
	}

	/*
	 * If the erase size is less than the total size of the flash, use
	 * sector erase command
	 */

	/*
	 * Calculate no. of sectors to erase based on byte count
	 */
	NumSect = ByteCount/(gFlash_Config_Table[gFCTIndex].SectSize) + 1;

	/*
	 * If ByteCount to k sectors,
	 * but the address range spans from N to N+k+1 sectors, then
	 * increment no. of sectors to be erased
	 */

	if( ((Address + ByteCount) & gFlash_Config_Table[gFCTIndex].SectMask) ==
		((Address + (NumSect * gFlash_Config_Table[gFCTIndex].SectSize)) &
		gFlash_Config_Table[gFCTIndex].SectMask) ) {
		NumSect++;
	}

	for (Sector = 0; Sector < NumSect; Sector++) {

		/*
		 * Translate address based on type of connection
		 * If stacked assert the slave select based on address
		 */
		RealAddr = GetRealAddr(QspiPsuPtr, Address);

		/*
		 * Send the write enable command to the Flash so that it can be
		 * written to, this needs to be sent as a separate transfer before
		 * the write
		 */
		gFlashMsg[0].TxBfrPtr = &WriteEnableCmd;
		gFlashMsg[0].RxBfrPtr = NULL;
		gFlashMsg[0].ByteCount = 1;
		gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		CmdBfr[COMMAND_OFFSET]   = gSectorEraseCmd;

		/*
		 * To be used only if 4B address sector erase cmd is
		 * supported by flash
		 */
		if(gFlash_Config_Table[gFCTIndex].FlashDeviceSize > SIXTEENMB) {
			CmdBfr[ADDRESS_1_OFFSET] = (u8)((RealAddr & 0xFF000000) >> 24);
			CmdBfr[ADDRESS_2_OFFSET] = (u8)((RealAddr & 0xFF0000) >> 16);
			CmdBfr[ADDRESS_3_OFFSET] = (u8)((RealAddr & 0xFF00) >> 8);
			CmdBfr[ADDRESS_4_OFFSET] = (u8)(RealAddr & 0xFF);
			gFlashMsg[0].ByteCount = 5;
		} else {
			CmdBfr[ADDRESS_1_OFFSET] = (u8)((RealAddr & 0xFF0000) >> 16);
			CmdBfr[ADDRESS_2_OFFSET] = (u8)((RealAddr & 0xFF00) >> 8);
			CmdBfr[ADDRESS_3_OFFSET] = (u8)(RealAddr & 0xFF);
			gFlashMsg[0].ByteCount = 4;
		}

		gFlashMsg[0].TxBfrPtr = CmdBfr;
		gFlashMsg[0].RxBfrPtr = NULL;
		gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		/*
		 * Wait for the erase command to be completed
		 */
		while (1) {
			ReadStatusCmd = gStatusCmd;
			gFlashMsg[0].TxBfrPtr = &ReadStatusCmd;
			gFlashMsg[0].RxBfrPtr = NULL;
			gFlashMsg[0].ByteCount = 1;
			gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

			gFlashMsg[1].TxBfrPtr = NULL;
			gFlashMsg[1].RxBfrPtr = FlashStatus;
			gFlashMsg[1].ByteCount = 2;
			gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			gFlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;
			if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
				gFlashMsg[1].Flags |= XQSPIPSU_MSG_FLAG_STRIPE;
			}

			Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 2);
			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}

			if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
				if(gFSRFlag) {
					FlashStatus[1] &= FlashStatus[0];
				} else {
					FlashStatus[1] |= FlashStatus[0];
				}
			}

			if(gFSRFlag) {
				if ((FlashStatus[1] & 0x80) != 0) {
					break;
				}
			} else {
				if ((FlashStatus[1] & 0x01) == 0) {
					break;
				}
			}
		}
		Address += gFlash_Config_Table[gFCTIndex].SectSize;
	}

	return 0;
}


/*****************************************************************************/
/**
*
* This function performs read. DMA is the default setting.
*
* @param	QspiPtr is a pointer to the QSPIPSU driver component to use.
* @param	Address contains the address of the first sector which needs to
*			be erased.
* @param	ByteCount contains the total size to be erased.
* @param	Pointer to the write buffer which contains data to be transmitted
* @param	Pointer to the read buffer to which valid received data should be
* 			written
*
* @return	XST_SUCCESS if successful, else XST_FAILURE.
*
* @note		None.
*
******************************************************************************/
int FlashRead(XQspiPsu *QspiPsuPtr, u32 Address, u32 ByteCount, u8 *ReadBfrPtr)
{
	u32 RealAddr;
	u32 DiscardByteCnt;
	u32 FlashMsgCnt;
	int Status;
	u8 Command = gReadCmd;
	u8 CmdBfr[CMD_BFR_SIZE] = {0};

	/* Check die boundary conditions if required for any flash */

	/* For Dual Stacked, split and read for boundary crossing */
	/*
	 * Translate address based on type of connection
	 * If stacked assert the slave select based on address
	 */
	RealAddr = GetRealAddr(QspiPsuPtr, Address);

	CmdBfr[COMMAND_OFFSET] = Command;
	if(gFlash_Config_Table[gFCTIndex].FlashDeviceSize > SIXTEENMB) {
		CmdBfr[ADDRESS_1_OFFSET] = (u8)((RealAddr & 0xFF000000) >> 24);
		CmdBfr[ADDRESS_2_OFFSET] = (u8)((RealAddr & 0xFF0000) >> 16);
		CmdBfr[ADDRESS_3_OFFSET] = (u8)((RealAddr & 0xFF00) >> 8);
		CmdBfr[ADDRESS_4_OFFSET] = (u8)(RealAddr & 0xFF);
		DiscardByteCnt = 5;
	} else {
		CmdBfr[ADDRESS_1_OFFSET] = (u8)((RealAddr & 0xFF0000) >> 16);
		CmdBfr[ADDRESS_2_OFFSET] = (u8)((RealAddr & 0xFF00) >> 8);
		CmdBfr[ADDRESS_3_OFFSET] = (u8)(RealAddr & 0xFF);
		DiscardByteCnt = 4;
	}

	gFlashMsg[0].TxBfrPtr = CmdBfr;
	gFlashMsg[0].RxBfrPtr = NULL;
	gFlashMsg[0].ByteCount = DiscardByteCnt;
	gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

	FlashMsgCnt = 1;

	/* It is recommended to have a separate entry for dummy */
	if ((Command == FAST_READ_CMD) || (Command == DUAL_READ_CMD) ||
	    (Command == QUAD_READ_CMD) || (Command == FAST_READ_CMD_4B) ||
	    (Command == DUAL_READ_CMD_4B) || (Command == QUAD_READ_CMD_4B)) {
		/* Update Dummy cycles as per flash specs for QUAD IO */

		/*
		 * It is recommended that Bus width value during dummy
		 * phase should be same as data phase
		 */
		if ((Command == FAST_READ_CMD) || (Command == FAST_READ_CMD_4B)) {
			gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		}

		if ((Command == DUAL_READ_CMD) || (Command == DUAL_READ_CMD_4B)) {
			gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_DUALSPI;
		}

		if ((Command == QUAD_READ_CMD) || (Command == QUAD_READ_CMD_4B)) {
			gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_QUADSPI;
		}

		gFlashMsg[1].TxBfrPtr = NULL;
		gFlashMsg[1].RxBfrPtr = NULL;
		gFlashMsg[1].ByteCount = DUMMY_CLOCKS;
		gFlashMsg[1].Flags = 0;

		FlashMsgCnt++;
	}

	if ((Command == FAST_READ_CMD) || (Command == FAST_READ_CMD_4B)) {
		gFlashMsg[FlashMsgCnt].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	}

	if ((Command == DUAL_READ_CMD) || (Command == DUAL_READ_CMD_4B)) {
		gFlashMsg[FlashMsgCnt].BusWidth = XQSPIPSU_SELECT_MODE_DUALSPI;
	}

	if ((Command == QUAD_READ_CMD) || (Command == QUAD_READ_CMD_4B)) {
		gFlashMsg[FlashMsgCnt].BusWidth = XQSPIPSU_SELECT_MODE_QUADSPI;
	}

	gFlashMsg[FlashMsgCnt].TxBfrPtr = NULL;
	gFlashMsg[FlashMsgCnt].RxBfrPtr = ReadBfrPtr;
	gFlashMsg[FlashMsgCnt].ByteCount = ByteCount;
	gFlashMsg[FlashMsgCnt].Flags = XQSPIPSU_MSG_FLAG_RX;

	if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
		gFlashMsg[FlashMsgCnt].Flags |= XQSPIPSU_MSG_FLAG_STRIPE;
	}

	Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, FlashMsgCnt+1);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	return 0;
}


/*****************************************************************************/
/**
*
* This API can be used to write to a flash register.
*
* @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
* @param	ByteCount is the number of bytes to write.
* @param	Command is specific register write command.
* @param	WriteBfrPtr is the pointer to value to be written.
* @param	WrEn is a flag to mention if WREN has to be sent before write.
*
* @return	XST_SUCCESS if successful, else XST_FAILURE.
*
* @note		This API can only be used for one flash at a time.
*
******************************************************************************/
int FlashRegisterWrite(XQspiPsu *QspiPsuPtr, u32 ByteCount, u8 Command,
					u8 *WriteBfrPtr, u8 WrEn)
{
	u8 WriteEnableCmd;
	u8 ReadStatusCmd;
	u8 FlashStatus[2];
	int Status;

	if(WrEn) {
		WriteEnableCmd = WRITE_ENABLE_CMD;

		/*
		 * Send the write enable command to the Flash so that it can be
		 * written to, this needs to be sent as a separate transfer before
		 * the write
		 */
		gFlashMsg[0].TxBfrPtr = &WriteEnableCmd;
		gFlashMsg[0].RxBfrPtr = NULL;
		gFlashMsg[0].ByteCount = 1;
		gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
	}

	WriteBfrPtr[COMMAND_OFFSET]   = Command;
	/*
	 * Value(s) is(are) expected to be written to the write buffer by calling API
	 * ByteCount is the count of the value(s) excluding the command.
	 */

	gFlashMsg[0].TxBfrPtr = WriteBfrPtr;
	gFlashMsg[0].RxBfrPtr = NULL;
	gFlashMsg[0].ByteCount = ByteCount + 1;
	gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

	Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Wait for the register write command to the Flash to be completed.
	 */
	while (1) {
		ReadStatusCmd = gStatusCmd;
		gFlashMsg[0].TxBfrPtr = &ReadStatusCmd;
		gFlashMsg[0].RxBfrPtr = NULL;
		gFlashMsg[0].ByteCount = 1;
		gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		gFlashMsg[1].TxBfrPtr = NULL;
		gFlashMsg[1].RxBfrPtr = FlashStatus;
		gFlashMsg[1].ByteCount = 2;
		gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 2);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		if(gFSRFlag) {
			if ((FlashStatus[1] & 0x80) != 0) {
				break;
			}
		} else {
			if ((FlashStatus[1] & 0x01) == 0) {
				break;
			}
		}
	}

	return 0;
}

/*****************************************************************************/
/**
*
* This API can be used to write to a flash register.
*
* @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
* @param	ByteCount is the number of bytes to write.
* @param	Command is specific register write command.
* @param	WriteBfrPtr is the pointer to value to be written.
* @param	WrEn is a flag to mention if WREN has to be sent before write.
*
* @return	XST_SUCCESS if successful, else XST_FAILURE.
*
* @note		This API can only be used for one flash at a time.
*
******************************************************************************/
int FlashRegisterRead(XQspiPsu *QspiPsuPtr, u32 ByteCount, u8 Command, u8 *ReadBfrPtr)
{
	u8 WriteCmd;
	int Status;

	WriteCmd = Command;
	gFlashMsg[0].TxBfrPtr = &WriteCmd;
	gFlashMsg[0].RxBfrPtr = NULL;
	gFlashMsg[0].ByteCount = 1;
	gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

	gFlashMsg[1].TxBfrPtr = NULL;
	gFlashMsg[1].RxBfrPtr = ReadBfrPtr;
	/* This is for DMA reasons; to be changed shortly */
	gFlashMsg[1].ByteCount = 4;
	gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;

	Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 2);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	return 0;
}

/*****************************************************************************/
/**
*
* This functions performs a bulk erase operation when the
* flash device has a single die. Works for both Spansion and Micron
*
* @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
* @param	WritBfrPtr is the pointer to command+address to be sent
*
* @return	XST_SUCCESS if successful, else XST_FAILURE.
*
* @note		None.
*
******************************************************************************/
int BulkErase(XQspiPsu *QspiPsuPtr, u8 *CmdBfrPtr)
{
	u8 WriteEnableCmd;
	u8 ReadStatusCmd;
	u8 FlashStatus[2];
	int Status;

	WriteEnableCmd = WRITE_ENABLE_CMD;
	/*
	 * Send the write enable command to the Flash so that it can be
	 * written to, this needs to be sent as a separate transfer before
	 * the write
	 */
	gFlashMsg[0].TxBfrPtr = &WriteEnableCmd;
	gFlashMsg[0].RxBfrPtr = NULL;
	gFlashMsg[0].ByteCount = 1;
	gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

	Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	CmdBfrPtr[COMMAND_OFFSET]   = BULK_ERASE_CMD;
	gFlashMsg[0].TxBfrPtr = CmdBfrPtr;
	gFlashMsg[0].RxBfrPtr = NULL;
	gFlashMsg[0].ByteCount = 1;
	gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

	Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Wait for the write command to the Flash to be completed, it takes
	 * some time for the data to be written
	 */
	while (1) {
		ReadStatusCmd = gStatusCmd;
		gFlashMsg[0].TxBfrPtr = &ReadStatusCmd;
		gFlashMsg[0].RxBfrPtr = NULL;
		gFlashMsg[0].ByteCount = 1;
		gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		gFlashMsg[1].TxBfrPtr = NULL;
		gFlashMsg[1].RxBfrPtr = FlashStatus;
		gFlashMsg[1].ByteCount = 2;
		gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;
		if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
			gFlashMsg[1].Flags |= XQSPIPSU_MSG_FLAG_STRIPE;
		}


		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 2);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}
		if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
			if(gFSRFlag) {
				FlashStatus[1] &= FlashStatus[0];
			} else {
				FlashStatus[1] |= FlashStatus[0];
			}
		}

		if(gFSRFlag) {
			if ((FlashStatus[1] & 0x80) != 0) {
				break;
			}
		} else {
			if ((FlashStatus[1] & 0x01) == 0) {
				break;
			}
		}
	}

	return 0;
}

/*****************************************************************************/
/**
*
* This functions performs a die erase operation on all the die in
* the flash device. This function uses the die erase command for
* Micron 512Mbit and 1Gbit
*
* @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
* @param	CmdBfrPtr is the pointer to command+address to be sent
*
* @return	XST_SUCCESS if successful, else XST_FAILURE.
*
* @note		None.
*
******************************************************************************/
int DieErase(XQspiPsu *QspiPsuPtr, u8 *CmdBfrPtr)
{
	u8 WriteEnableCmd;
	u8 DieCnt;
	u8 ReadStatusCmd;
	u8 FlashStatus[2];
	int Status;

	WriteEnableCmd = WRITE_ENABLE_CMD;
	for(DieCnt = 0; DieCnt < gFlash_Config_Table[gFCTIndex].NumDie; DieCnt++) {
		/*
		 * Send the write enable command to the Flash so that it can be
		 * written to, this needs to be sent as a separate transfer before
		 * the write
		 */
		gFlashMsg[0].TxBfrPtr = &WriteEnableCmd;
		gFlashMsg[0].RxBfrPtr = NULL;
		gFlashMsg[0].ByteCount = 1;
		gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		CmdBfrPtr[COMMAND_OFFSET]   = DIE_ERASE_CMD;
		/* Check these number of address bytes as per flash device */
		CmdBfrPtr[ADDRESS_1_OFFSET] = 0;
		CmdBfrPtr[ADDRESS_2_OFFSET] = 0;
		CmdBfrPtr[ADDRESS_3_OFFSET] = 0;

		gFlashMsg[0].TxBfrPtr = CmdBfrPtr;
		gFlashMsg[0].RxBfrPtr = NULL;
		gFlashMsg[0].ByteCount = 4;
		gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		/*
		 * Wait for the write command to the Flash to be completed, it takes
		 * some time for the data to be written
		 */
		while (1) {
			ReadStatusCmd = gStatusCmd;
			gFlashMsg[0].TxBfrPtr = &ReadStatusCmd;
			gFlashMsg[0].RxBfrPtr = NULL;
			gFlashMsg[0].ByteCount = 1;
			gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

			gFlashMsg[1].TxBfrPtr = NULL;
			gFlashMsg[1].RxBfrPtr = FlashStatus;
			gFlashMsg[1].ByteCount = 2;
			gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			gFlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;
			if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
				gFlashMsg[1].Flags |= XQSPIPSU_MSG_FLAG_STRIPE;
			}

			Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 2);
			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}
			if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
				if(gFSRFlag) {
					FlashStatus[1] &= FlashStatus[0];
				} else {
				FlashStatus[1] |= FlashStatus[0];
				}
			}

			if(gFSRFlag) {
				if ((FlashStatus[1] & 0x80) != 0) {
					break;
				}
			} else {
				if ((FlashStatus[1] & 0x01) == 0) {
					break;
				}
			}
		}
	}

	return 0;
}

/*****************************************************************************/
/**
*
* This functions translates the address based on the type of interconnection.
* In case of stacked, this function asserts the corresponding slave select.
*
* @param	QspiPsuPtr is a pointer to the QSPIPSU driver component to use.
* @param	Address which is to be accessed (for erase, write or read)
*
* @return	RealAddr is the translated address - for single it is unchanged;
* 			for stacked, the lower flash size is subtracted;
* 			for parallel the address is divided by 2.
*
* @note		None.
*
******************************************************************************/
u32 GetRealAddr(XQspiPsu *QspiPsuPtr, u32 Address)
{
	u32 RealAddr;

	switch(QspiPsuPtr->Config.ConnectionMode) {
	case XQSPIPSU_CONNECTION_MODE_SINGLE:
		XQspiPsu_SelectFlash(QspiPsuPtr, XQSPIPSU_SELECT_FLASH_CS_LOWER,
					XQSPIPSU_SELECT_FLASH_BUS_LOWER);
		RealAddr = Address;
		break;
	case XQSPIPSU_CONNECTION_MODE_STACKED:
		/* Select lower or upper Flash based on sector address */
		if(Address & gFlash_Config_Table[gFCTIndex].FlashDeviceSize) {

			XQspiPsu_SelectFlash(QspiPsuPtr,
				XQSPIPSU_SELECT_FLASH_CS_UPPER, XQSPIPSU_SELECT_FLASH_BUS_LOWER);
			/*
			 * Subtract first flash size when accessing second flash
			 */
			RealAddr = Address &
					(~gFlash_Config_Table[gFCTIndex].FlashDeviceSize);
		}else{
			/*
			 * Set selection to L_PAGE
			 */
			XQspiPsu_SelectFlash(QspiPsuPtr,
				XQSPIPSU_SELECT_FLASH_CS_LOWER, XQSPIPSU_SELECT_FLASH_BUS_LOWER);

			RealAddr = Address;

		}
		break;
	case XQSPIPSU_CONNECTION_MODE_PARALLEL:
		/*
		 * The effective address in each flash is the actual
		 * address / 2
		 */
		XQspiPsu_SelectFlash(QspiPsuPtr,
			XQSPIPSU_SELECT_FLASH_CS_BOTH, XQSPIPSU_SELECT_FLASH_BUS_BOTH);
		RealAddr = Address / 2;
		break;
	default:
		/* RealAddr wont be assigned in this case; */
	break;

	}

	return(RealAddr);
}

/*****************************************************************************/
/**
* @brief
* This API enters the flash device into 4 bytes addressing mode.
* As per the Micron and ISSI spec, before issuing the command to enter into 4 byte addr
* mode, a write enable command is issued. For Macronix and Winbond flash parts write
* enable is not required.
*
* @param	QspiPtr is a pointer to the QSPIPSU driver component to use.
* @param	Enable is a either 1 or 0 if 1 then enters 4 byte if 0 exits.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if it fails.
*
*
******************************************************************************/
int FlashEnterExit4BAddMode(XQspiPsu *QspiPsuPtr,unsigned int Enable)
{
	int Status;
	u8 WriteEnableCmd;
	u8 Cmd;
	u8 WriteDisableCmd;
	u8 ReadStatusCmd;
	u8 WriteBuffer[2] = {0};
	u8 FlashStatus[2] = {0};

	if(Enable) {
		Cmd = ENTER_4B_ADDR_MODE;
	} else {
		if(gFlashMake == ISSI_ID_BYTE0)
			Cmd = EXIT_4B_ADDR_MODE_ISSI;
		else
			Cmd = EXIT_4B_ADDR_MODE;
	}

	switch (gFlashMake) {
		case ISSI_ID_BYTE0:
		case MICRON_ID_BYTE0:
			WriteEnableCmd = WRITE_ENABLE_CMD;
			/*
			 * Send the write enable command to the Flash so that it can be
			 * written to, this needs to be sent as a separate transfer before
			 * the write
			 */
			gFlashMsg[0].TxBfrPtr = &WriteEnableCmd;
			gFlashMsg[0].RxBfrPtr = NULL;
			gFlashMsg[0].ByteCount = 1;
			gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

			Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}
			break;

		case SPANSION_ID_BYTE0:

			if(Enable) {
				WriteBuffer[0] = BANK_REG_WR;
				WriteBuffer[1] = 1 << 7;
			} else {
				WriteBuffer[0] = BANK_REG_WR;
				WriteBuffer[1] = 0 << 7;
			}

			gFlashMsg[0].TxBfrPtr = WriteBuffer;
			gFlashMsg[0].RxBfrPtr = NULL;
			gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;
			gFlashMsg[0].ByteCount = 2;

			Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}

			return Status;

		default:
			/*
			 * For Macronix and Winbond flash parts
			 * Write enable command is not required.
			 */
			break;
	}


	gFlashMsg[0].TxBfrPtr = &Cmd;
	gFlashMsg[0].RxBfrPtr = NULL;
	gFlashMsg[0].ByteCount = 1;
	gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
	gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

	Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	while (1) {
		ReadStatusCmd = gStatusCmd;

		gFlashMsg[0].TxBfrPtr = &ReadStatusCmd;
		gFlashMsg[0].RxBfrPtr = NULL;
		gFlashMsg[0].ByteCount = 1;
		gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

		gFlashMsg[1].TxBfrPtr = NULL;
		gFlashMsg[1].RxBfrPtr = FlashStatus;
		gFlashMsg[1].ByteCount = 2;
		gFlashMsg[1].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
		gFlashMsg[1].Flags = XQSPIPSU_MSG_FLAG_RX;

		if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
			gFlashMsg[1].Flags |= XQSPIPSU_MSG_FLAG_STRIPE;
		}

		Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 2);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		if(QspiPsuPtr->Config.ConnectionMode == XQSPIPSU_CONNECTION_MODE_PARALLEL){
			if(gFSRFlag) {
				FlashStatus[1] &= FlashStatus[0];
			} else {
				FlashStatus[1] |= FlashStatus[0];
			}
		}

		if(gFSRFlag) {
			if ((FlashStatus[1] & 0x80) != 0) {
				break;
			}
		} else {
			if ((FlashStatus[1] & 0x01) == 0) {
				break;
			}
		}
	}

	switch (gFlashMake) {
		case ISSI_ID_BYTE0:
		case MICRON_ID_BYTE0:
			WriteDisableCmd = WRITE_DISABLE_CMD;
			/*
			 * Send the write enable command to the Flash so that it can be
			 * written to, this needs to be sent as a separate transfer before
			 * the write
			 */
			gFlashMsg[0].TxBfrPtr = &WriteDisableCmd;
			gFlashMsg[0].RxBfrPtr = NULL;
			gFlashMsg[0].ByteCount = 1;
			gFlashMsg[0].BusWidth = XQSPIPSU_SELECT_MODE_SPI;
			gFlashMsg[0].Flags = XQSPIPSU_MSG_FLAG_TX;

			Status = XQspiPsu_PolledTransfer(QspiPsuPtr, gFlashMsg, 1);
			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}
			break;

		default:
			/*
			 * For Macronix and Winbond flash parts
			 * Write disable command is not required.
			 */
			break;
	}

	return Status;
}
