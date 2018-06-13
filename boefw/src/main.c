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
#include "flash_oper.h"
#include "version.h"

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
	UPGRADE_RECVING,
    UPGRADE_RECV_FIN,
    UPGRADE_ERASEING_FLASH,
	UPGRADE_ERASED_FLASH,
    UPGRADE_WRITEING_FLASH,
	UPGRADE_WRITE_FLASH_FIN,
    UPGRADE_REBOOT = 0xA,
    UPGRADE_ABORT = 0xF,
}UPGRADE_FLAG;
extern GlobalHandle gHandle;

static int make_response_version(A_Package *req, ACmd cmd, u8 version, A_Package *p)
{
    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, &version, 1);
    axu_finish_package(p);
    return 0;
}

static int make_response_random(A_Package *req, ACmd cmd, A_Package *p)
{
    axu_package_init(p, req, cmd);
    uint32_t r = genrandom();
    axu_set_data(p, 0, (u8*)&r, sizeof(r));
    axu_finish_package(p);
    return 0;
}

static int make_package_progress(ACmd cmd, u8 progress, A_Package *p)
{
    axu_package_init(p, NULL, cmd);
    axu_set_data(p, 0, &progress, sizeof(progress));
    axu_finish_package(p);
    return 0;
}

static int make_response_ack(A_Package *req, ACmd cmd, u8 ack, A_Package *p)
{
    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, &ack, sizeof(ack));
    axu_finish_package(p);
    return 0;
}

static int make_response_boeid(A_Package *req, ACmd cmd, A_Package *p)
{
    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, (u8*)&gHandle.gEnv.boeid, sizeof(gHandle.gEnv.boeid));
    axu_finish_package(p);
    return 0;
}

static int make_response_error(A_Package *req, ACmd cmd, u8 err_code, char*err_info, int len, A_Package *p)
{
    int offset = 0;
    axu_package_init(p, req, cmd);
    axu_set_data(p, offset, &err_code, sizeof(err_code));
    offset += sizeof(err_code);
    axu_set_data(p, offset, (u8*)err_info, len);
    axu_finish_package(p);
    return 0;
}

#define TRANSPORT_START_BASE            (0)
#define TRANSPORT_START_USAGE_SIZE      (1)
#define TRANSPORT_START_ID_SIZE         (4)
#define TRANSPORT_START_CHECKSUM_SIZE   (4)
#define TRANSPORT_START_DATALEN_SIZE    (4)
#define GetStartUsage(pack)             (pack->data[0])
#define GetStartID(pack)                (*(u32*)(&(pack->data[1])))
#define GetStartCHK(pack)               (*(u32*)(&(pack->data[1+4])))
#define GetStartTotallen(pack)          (*(u32*)(&(pack->data[1+4+4])))


static int doTransportStart(A_Package *p, A_Package *res)
{
    u32 fileid = 0, datalen = 0;
    char *errmsg = NULL;
    FPartation fp;
    fileid = GetStartID(p);
    datalen = GetStartTotallen(p);

    // remove old info.
    BlockDataInfo *info = findInfo(&(gHandle.gBlockDataList), fileid);
    if(info != NULL){
        removeInfo(&gHandle.gBlockDataList, info);
        info = NULL;
    }
    // check usage.
    BlockDataUsage usage = GetStartUsage(p);
    if(usage >= BD_USE_END || usage <= BD_USE_START){
        errmsg = axu_get_error_msg(A_STA_USAGE_ERROR);
        make_response_error(p, ACMD_BP_RES_ERR, A_STA_USAGE_ERROR, errmsg, strlen(errmsg), res);
        return 1;
    }
    
    info = (BlockDataInfo*)malloc(datalen + sizeof(BlockDataInfo));
    memset(info, 0x0, datalen + sizeof(BlockDataInfo));
    info->uniqueId = fileid;
    info->checksum = GetStartCHK(p);
    info->dataLen = datalen;
    info->flag = UPGRADE_RECVING;
    // check datalen.
    if(usage == BD_USE_UPGRADE_FW){
        FM_GetPartation(FM_IMAGE1_PNAME, &fp);
        if(datalen > fp.pSize){
            errmsg = axu_get_error_msg(A_STA_LEN_ERROR);
            make_response_error(p, ACMD_BP_RES_ERR, A_STA_LEN_ERROR, errmsg, strlen(errmsg), res);
            return 1;
        }
    }else if(usage == BD_USE_UPGRADE_GOLDEN){
        FM_GetPartation(FM_GOLDEN_PNAME, &fp);
        if(datalen > fp.pSize){
            errmsg = axu_get_error_msg(A_STA_LEN_ERROR);
            make_response_error(p, ACMD_BP_RES_ERR, A_STA_LEN_ERROR, errmsg, strlen(errmsg), res);
            return 1;
        }
    }

    addInfo(&gHandle.gBlockDataList, info);
    make_response_ack(p, ACMD_BP_RES_ACK, 1, res);

    return 0;
}

#define TRANSPORT_MID_ID_SIZE (4)
#define TRANSPORT_MID_OFFSET_SIZE (2)
#define TRANSPORT_MID_DATA_OFFSET (TRANSPORT_MID_ID_SIZE+TRANSPORT_MID_OFFSET_SIZE)
#define GetMidID(pack)        (*((u32*)(&pack->data[0])))
#define GetMidOffset(pack)    (*((u16*)(&pack->data[0+4])))
#define GetMidDatalen(pack)   ((pack)->header.body_length - (0+4+2))

static int doTransportMid(A_Package *p, A_Package *res)
{
    u32 fileid = 0, datalen = 0;
    u16 offset = 0;

    char *errmsg = NULL;
    fileid = GetMidID(p);
    offset = GetMidOffset(p);
    datalen = GetMidDatalen(p);

    // find info.
    BlockDataInfo *info = findInfo(&(gHandle.gBlockDataList), fileid);
    if(info == NULL){
        errmsg = axu_get_error_msg(A_MID_UID_NOT_FOUND);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_UID_NOT_FOUND, errmsg, strlen(errmsg), res);
        return 1;
    }

    // check datalen.
    if((offset + datalen) > info->dataLen){
        errmsg = axu_get_error_msg(A_MID_LEN_ERROR);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_LEN_ERROR, errmsg, strlen(errmsg), res);
        return 1;
    }
    // restructure data.
    memcpy(&(info->data[offset]), &(p->data[TRANSPORT_MID_DATA_OFFSET]), datalen);
    make_response_ack(p, ACMD_BP_RES_ACK, 1, res);

    return 0;
}

#define TRANSPORT_FIN_ID_SIZE (4)
#define TRANSPORT_FIN_OFFSET_SIZE (2)
#define TRANSPORT_FIN_DATA_OFFSET (TRANSPORT_MID_ID_SIZE+TRANSPORT_MID_OFFSET_SIZE)
#define GetFinID(pack)        (*((u32*)(&pack->data[0])))
#define GetFinOffset(pack)    (*((u16*)(&pack->data[0+4])))
#define GetFinDatalen(pack)   ((pack)->header.body_length - (0+4+2))
static int doTransportFin(A_Package *p, A_Package *res)
{
    u32 fileid = 0, datalen = 0;
    u16 offset = 0;

    char *errmsg = NULL;
    fileid = GetFinID(p);
    offset = GetFinOffset(p);
    datalen = GetFinDatalen(p);

    // find info.
    BlockDataInfo *info = findInfo(&(gHandle.gBlockDataList), fileid);
    if(info == NULL){
        errmsg = axu_get_error_msg(A_MID_UID_NOT_FOUND);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_UID_NOT_FOUND, errmsg, strlen(errmsg), res);
        return 1;
    }

    // check datalen.
    if((offset + datalen) > info->dataLen){
        errmsg = axu_get_error_msg(A_MID_LEN_ERROR);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_LEN_ERROR, errmsg, strlen(errmsg), res);
        return 1;
    }
    // restructure data.
    memcpy(&(info->data[offset]), &(p->data[TRANSPORT_MID_DATA_OFFSET]), datalen);
    u32 chk = checksum(info->data, info->dataLen);
    if(chk != info->checksum){
        errmsg = axu_get_error_msg(A_FIN_CHECKSUM_ERROR);
        make_response_error(p, ACMD_BP_RES_ERR, A_FIN_CHECKSUM_ERROR, errmsg, strlen(errmsg), res);
        return 1;
    }
    info->flag = UPGRADE_RECV_FIN;

    make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
    return 0;
}

#define UPGRADE_ID_SIZE (4)
#define GetUpgradeID(pack)        (*((u32*)(&pack->data[0])))
static int doUpgradeStart(A_Package *p, A_Package *res)
{
    u32 fileid = 0;
    u32 bootAddr = gHandle.gEnv.bootaddr;
    u32 upgradeAddr = 0x0, writeLen = 0x0;
    FPartation fp_golden, fp_img1, fp_img2;

    char *errmsg = NULL;
    fileid = GetUpgradeID(p);

    // find info.
    BlockDataInfo *info = findInfo(&(gHandle.gBlockDataList), fileid);
    if(info == NULL){
        errmsg = axu_get_error_msg(A_MID_UID_NOT_FOUND);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_UID_NOT_FOUND, errmsg, strlen(errmsg), res);
        return 1;
    }

    if(info->flag != UPGRADE_RECV_FIN){
    	errmsg = axu_get_error_msg(A_UPGRADE_STATE_ERROR);
    	make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_STATE_ERROR, errmsg, strlen(errmsg), res);
    	return 1;
    }

    // find write add
    FM_GetPartation(FM_GOLDEN_PNAME, &fp_golden);
    FM_GetPartation(FM_IMAGE1_PNAME, &fp_img1);
    FM_GetPartation(FM_IMAGE2_PNAME, &fp_img2);
    if(info->eDataUsage == BD_USE_UPGRADE_FW){
        upgradeAddr = fp_golden.pAddr;
        writeLen = fp_golden.pSize;
    }else if(info->eDataUsage == BD_USE_UPGRADE_FW) {
        if(fp_img1.pAddr == bootAddr){
            upgradeAddr = fp_img2.pAddr;
            writeLen = fp_img2.pSize;
        }else {
            upgradeAddr = fp_img1.pAddr;
            writeLen = fp_img1.pSize;
        }
    }
    // flash erase.
    if(0 != FlashErase(&(gHandle.gFlashInstance), upgradeAddr, writeLen)){
        xil_printf("Upgrade: flash erase error, addr:0x%x, size:0x%x.\r\n", upgradeAddr, writeLen);
        errmsg = axu_get_error_msg(A_UPGRADE_FLASH_ERASE_ERROR);
        make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_FLASH_ERASE_ERROR, errmsg, strlen(errmsg), res);
        return 1;
    }
    // write to flash
    if(0 != FlashWrite(&(gHandle.gFlashInstance), upgradeAddr, info->dataLen, info->data)){
        xil_printf("Upgrade: flash write error, addr:0x%x, size:0x%x.\r\n", upgradeAddr, info->data);
        errmsg = axu_get_error_msg(A_UPGRADE_FLASH_WRITE_ERROR);
        make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_FLASH_WRITE_ERROR, errmsg, strlen(errmsg), res);
        return 1;
    }

    gHandle.gEnv.bootaddr = upgradeAddr;
    gHandle.gEnv.update_flag = UPGRADE_REBOOT;
    if(XST_SUCCESS == env_update(&(gHandle.gEnv))){
    	make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
    }else{
    	errmsg = axu_get_error_msg(A_UPGRADE_ENV_UPDATE_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_ENV_UPDATE_ERROR, errmsg, strlen(errmsg), res);
		return 1;
    }
    // Todo: reset board or not.

    return 0;
}

static int doUpgradeAbort(A_Package *p, A_Package *res)
{
    u32 fileid = 0;

    char *errmsg = NULL;
    fileid = GetUpgradeID(p);

    // find info.
    BlockDataInfo *info = findInfo(&(gHandle.gBlockDataList), fileid);
    if(info == NULL){
        errmsg = axu_get_error_msg(A_MID_UID_NOT_FOUND);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_UID_NOT_FOUND, errmsg, strlen(errmsg), res);
        return 1;
    }
    if(info->flag < UPGRADE_WRITEING_FLASH){
        info->flag = UPGRADE_ABORT;
        make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
    }else {
        errmsg = axu_get_error_msg(A_UPGRADE_ABORT_ERROR);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_UID_NOT_FOUND, errmsg, strlen(errmsg), res);
    }

    return 0;
}

static int doGetVersion(A_Package *p, A_Package *res)
{
    int offset = 0;
    axu_package_init(res, p, ACMD_BP_RES_VERSION);
    axu_set_data(res, offset, &gHWVersion, 1);
    offset += 1;
    axu_set_data(res, offset, &gBFVersion, 1);
    offset += 1;
    axu_set_data(res, offset, &gAXUVersion, 1);
    offset += 1;
    axu_finish_package(res);
    return 0;
}

/*
 * return value: 0: correct data.
 *               1: magic error.
 *               2: checksum error.
 */
static int package_check(A_Package *pack)
{
    static u64 err_num;
    int ret = 0;
    if(pack->header.magic_aacc == 0xaacc && pack->header.magic_ccaa == 0xccaa){
        // compare checksum.
        u32 cks = checksum(pack->data, pack->header.body_length);
        if(cks != pack->checksum){
            ret = 2;
            err_num++;
        }
    }else {
        ret = 1;
        err_num++;
    }

    return ret;
}


int mainloop(void)
{
	A_Package *send = axu_package_new(PACKAGE_MIN_SIZE);
    char *errmsg = NULL;
    int ret = 0;
    while(gHandle.bRun){
        A_Package *rcv = NULL;
        u32 timeout_ms = 2;
        if(0 != msg_pool_fetch(&gHandle.gMsgPoolIns, &rcv, timeout_ms)) {
            // have no msg.
        }else {
            ret = package_check(rcv);
            if(0 == ret)
            {
                ACmd cmd = rcv->header.acmd;
                switch(cmd){
                    case ACMD_PB_GET_VERSION_INFO:
                        doGetVersion(rcv, send);
                        msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        break;
                    case ACMD_PB_TRANSPORT_START:
                        doTransportStart(rcv, send);
                        msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        break;
                    case ACMD_PB_TRANSPORT_MIDDLE:
                        doTransportMid(rcv, send);
                        msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        break;
                    case ACMD_PB_TRANSPORT_FINISH:
                        doTransportFin(rcv, send);
                        msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        break;
                    case ACMD_PB_UPGRADE_START:
                        doUpgradeStart(rcv, send);
                        msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        break;
                    case ACMD_PB_UPGRADE_ABORT:
                        doUpgradeAbort(rcv, send);
                        msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        break;
                    case ACMD_PB_RESET:
                        // do board reset.
                        break;
                    case ACMD_PB_GET_RANDOM:
                        make_response_random(rcv, ACMD_BP_RES_RANDOM,send);
                        msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        break;
                    case ACMD_PB_GET_BOEID:
                        make_response_boeid(rcv, ACMD_BP_RES_BOEID,send);
                        msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        break;
                    case ACMD_PB_GET_HW_VER:
                        make_response_version(rcv, ACMD_BP_RES_HW_VER, gHWVersion, send);
                        msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        break;
                    case ACMD_PB_GET_FW_VER:
                        make_response_version(rcv, ACMD_BP_RES_FW_VER, gBFVersion, send);
                        msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        break;
                    case ACMD_PB_GET_AXU_VER:
                        make_response_version(rcv, ACMD_BP_RES_AXU_VER, gAXUVersion, send);
                        msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        break;
                    case ACMD_PB_SET_BOEID:
                        {
                            u32 nBoeID = 0;
                            memcpy(&nBoeID, rcv->data, sizeof(u32));
                            gHandle.gEnv.boeid = nBoeID;
                            if(env_update(&(gHandle.gEnv)) == 0) {
                                make_response_ack(rcv, ACMD_BP_RES_ACK, 1, send);
                            }else{
                                make_response_ack(rcv, ACMD_BP_RES_ACK, 0, send);
                            }
                            msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
                        }
                        break;
                    default :
                        xil_printf("Unsupport cmd %d.\n\r", cmd);
                        break;
                }
            }else if(2 == ret){
                // checksum error.
            	errmsg = axu_get_error_msg(A_CHECKSUM_ERROR);
                make_response_error(rcv, ACMD_BP_RES_ERR, A_CHECKSUM_ERROR, errmsg, strlen(errmsg), send);
                msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
            }else if(1 == ret){
                // magic error.
            	errmsg = axu_get_error_msg(A_MAGIC_ERROR);
                make_response_error(rcv, ACMD_BP_RES_ERR, A_MAGIC_ERROR, errmsg, strlen(errmsg), send);
                msg_pool_txsend(gHandle.gMsgPoolIns, send, PACKAGE_MIN_SIZE);
            }
        }
        // release rcv.
        axu_package_free(rcv);
    }
    return 0;
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
    // 2. check upgrade flag.
    if(gHandle.gEnv.update_flag == UPGRADE_REBOOT){
        // Now is reboot from upgrade, so upgrade successful.
        // response upgrade success.
        A_Package *pack = axu_package_new(PACKAGE_MIN_SIZE);
        gHandle.gEnv.update_flag = UPGRADE_NONE;
        env_update(&gHandle.gEnv);
        make_package_progress(ACMD_BP_RES_UPGRADE_PROGRESS, 100, pack);
        msg_pool_txsend(gHandle.gMsgPoolIns, pack, PACKAGE_MIN_SIZE);
        axu_package_free(pack);
    }

    xil_printf("Welcome to HPB.\r\n");

    // 3. enter mainloop.
    mainloop();



    cleanup_platform();
    return 0;
}
