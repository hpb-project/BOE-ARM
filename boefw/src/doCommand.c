/*
 * doCommand.c
 *
 *  Created on: Jun 13, 2018
 *      Author: luxq
 */
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "flash_oper.h"
#include "flash_map.h"
#include "version.h"
#include "doCommand.h"
#include "atimer.h"
#include "at508.h"
#include "emac_oper.h"

#define FPGA_VERSION_REG (0xa0000004)    // value format: 0x00010901 v1.9.1
static u8 gEmpty256[32] = {0};
static u8 CmdBfr[8];


static u64 pro_st, pro_so;
#define PROFILE_START()\
	pro_st = 0;\
	atimer_reset();\


#define PROFILE_STOP()\
	pro_so = atimer_gettm();\
	xil_printf("--PROFILE--time cost %dms.\r\n", pro_so - pro_st);

static void doMultiBoot(u32 Addr)
{
printf("doMultiBoot Addr %d \n",Addr);
    GoMultiBoot(Addr/0x8000);
}

static int make_response_version(A_Package *req, ACmd cmd, u8 version, A_Package *p)
{
    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, &version, 1);
    xil_printf("version = 0x%02x\r\n", version);
    axu_finish_package(p);
    return 0;
}

static int make_response_random(A_Package *req, ACmd cmd, A_Package *p)
{
	u8 rdm[32];

	int status = at508_get_random(rdm, sizeof(rdm));
	if(status != PRET_OK){
		for(int i = 0; i < 8; i++){
			u32 r = genrandom();
			memcpy(&rdm[i*4], &r, sizeof(r));
		}
	}

    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, rdm, sizeof(rdm));
    axu_finish_package(p);
    return 0;
}

int make_package_progress(ACmd cmd, u8 progress, char *msg, A_Package *p)
{
	int offset = 0;
    axu_package_init(p, NULL, cmd);
    axu_set_data(p, offset, &progress, sizeof(progress));
    offset += sizeof(progress);
    axu_set_data(p, offset, (u8*)msg, strlen(msg));
    axu_finish_package(p);
    return 0;
}

int make_response_ack(A_Package *req, ACmd cmd, u8 ack, A_Package *p)
{
    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, &ack, sizeof(ack));
    //xil_printf("response ack = %d.\r\n", ack);
    axu_finish_package(p);
    return 0;
}

static int make_response_sn(A_Package *req, ACmd cmd, A_Package *p)
{
    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, gHandle.gEnv.board_sn, sizeof(gHandle.gEnv.board_sn));
    xil_printf("response sn = %s\r\n", gHandle.gEnv.board_sn);
    axu_finish_package(p);
    return 0;
}

static int make_response_mac(A_Package *req, ACmd cmd, A_Package *p)
{
    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, gHandle.gEnv.board_mac, sizeof(gHandle.gEnv.board_mac));
    axu_finish_package(p);
    return 0;
}

static int make_response_phy_read(A_Package *req, ACmd cmd, u16 val, A_Package *p)
{
    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, (u8*)&val, sizeof(val));
    axu_finish_package(p);
    return 0;
}

static int make_response_reg_read(A_Package *req, ACmd cmd, u32 val, A_Package *p)
{
    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, (u8*)&val, sizeof(val));
    axu_finish_package(p);
    return 0;
}
static int make_response_random_read(A_Package *req, ACmd cmd, unsigned char val[32], A_Package *p)
{

    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, (u8 *)val, 32);
    axu_finish_package(p);
    return 0;
}


int make_response_error(A_Package *req, ACmd cmd, u8 err_code, char*err_info, int len, A_Package *p)
{
    int offset = 0;
    axu_package_init(p, req, cmd);
    axu_set_data(p, offset, &err_code, sizeof(err_code));
    offset += sizeof(err_code);
    axu_set_data(p, offset, (u8*)err_info, len);
    axu_finish_package(p);
    xil_printf("response err = %d, msg = %s.\r\n", err_code, err_info);
    return 0;
}

void send_upgrade_progress(u8 progress, char *msg)
{
	A_Package *pack = axu_package_new(PACKAGE_MIN_SIZE);
	make_package_progress(ACMD_BP_RES_UPGRADE_PROGRESS, progress, msg, pack);
	msg_pool_txsend(gHandle.gMsgPoolIns, pack, axu_package_len(pack));
	axu_package_free(pack);
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
#define GetHVersion(pack)				(pack->data[1+4+4+4])
#define GetMVersion(pack)				(pack->data[1+4+4+4+1])
#define GetFVersion(pack)				(pack->data[1+4+4+4+1+1])
#define GetDVersion(pack)				(pack->data[1+4+4+4+1+1+1])

static int checkVersion(A_Package *p)
{
	u8 nhVer = GetHVersion(p);
	u8 nmVer = GetMVersion(p);
	u8 nfVer = GetFVersion(p);
	u8 ndVer = GetDVersion(p);

	if((vMajor(nhVer) == vMajor(gVersion.H))
			&& (vMinor(nhVer) >= vMinor(gVersion.H)))
	{
		u32 nvcnt = nmVer << 16 | nfVer << 8 | ndVer;
		u32 ovcnt = gVersion.M << 16 | gVersion.F << 8 | gVersion.D;
		if(nvcnt > ovcnt)
			return 0;
	}
	return 1;
}
static PRET doTransportStart(A_Package *p, A_Package *res)
{
    u32 fileid = 0, datalen = 0;

    char *errmsg = NULL;
    char msgbuf[PACKAGE_MIN_SIZE] = {0};
    FPartation fp;
    fileid = GetStartID(p);
    datalen = GetStartTotallen(p);

    xil_printf("fid = 0x%x, len = %d.\r\n", fileid, datalen);
    // remove old info.
    BlockDataInfo *info = findInfo(&(gHandle.gBlockDataList), fileid);
    if(info != NULL){
    	xil_printf("remove old file info.\r\n");
        removeInfo(&gHandle.gBlockDataList, info);
        info = NULL;
    }
    // check usage.
    BlockDataUsage usage = GetStartUsage(p);
    if(usage >= BD_USE_END || usage <= BD_USE_START){
    	xil_printf("usage not support, %d.\r\n", usage);
        errmsg = axu_get_error_msg(A_STA_USAGE_ERROR);
        make_response_error(p, ACMD_BP_RES_ERR, A_STA_USAGE_ERROR, errmsg, strlen(errmsg), res);
        return PRET_ERROR;
    }

    // check version info, then create info.
    if(checkVersion(p) != 0){
    	xil_printf("check version error.\r\n");
    	errmsg = axu_get_error_msg(A_VERSION_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_VERSION_ERROR, errmsg, strlen(errmsg), res);
		return PRET_ERROR;
    }

    // new a blockdata info.
    info = (BlockDataInfo*)malloc(datalen + sizeof(BlockDataInfo));
    xil_printf("info = 0x%p\r\n", info);
    memset(info, 0x0, datalen + sizeof(BlockDataInfo));
    info->uniqueId = fileid;
    info->checksum = GetStartCHK(p);
    info->dataLen = datalen;
    info->flag = UPGRADE_RECVING;
    info->eDataUsage = usage;

    // check datalen.
    if(usage == BD_USE_UPGRADE_FW){
        FM_GetPartation(FM_IMAGE1_PNAME, &fp);
        if(datalen > fp.pSize){
            errmsg = axu_get_error_msg(A_STA_LEN_ERROR);
            strcat(msgbuf, errmsg);
            sprintf(msgbuf+strlen(errmsg), ": datalen = %d, partation size = %d.", info->dataLen, fp.pSize);
            make_response_error(p, ACMD_BP_RES_ERR, A_STA_LEN_ERROR, msgbuf, strlen(msgbuf), res);
            return PRET_ERROR;
        }
    }else if(usage == BD_USE_UPGRADE_GOLDEN){
        FM_GetPartation(FM_GOLDEN_PNAME, &fp);
        if(datalen > fp.pSize){
            errmsg = axu_get_error_msg(A_STA_LEN_ERROR);
            strcat(msgbuf, errmsg);
			sprintf(msgbuf+strlen(errmsg), ": datalen = %d, partation size = %d.", info->dataLen, fp.pSize);
			make_response_error(p, ACMD_BP_RES_ERR, A_STA_LEN_ERROR, msgbuf, strlen(msgbuf), res);
            return PRET_ERROR;
        }
    }

    addInfo(&gHandle.gBlockDataList, info);
    make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
    send_upgrade_progress(5, "start transport");

    return PRET_OK;
}

#define TRANSPORT_MID_ID_SIZE (4)
#define TRANSPORT_MID_OFFSET_SIZE (4)
#define TRANSPORT_MID_LEN_SIZE (4)
#define TRANSPORT_MID_DATA_OFFSET (TRANSPORT_MID_ID_SIZE+TRANSPORT_MID_OFFSET_SIZE+TRANSPORT_MID_LEN_SIZE)
#define GetMidID(pack)        (*((u32*)(&pack->data[0])))
#define GetMidOffset(pack)    (*((u32*)(&pack->data[0+4])))
#define GetMidDatalen(pack)   (*((u32*)(&pack->data[0+4+4])))

static PRET doTransportMid(A_Package *p, A_Package *res)
{
    u32 fileid = 0, datalen = 0;
    u32 offset = 0;

    char *errmsg = NULL;
    char msgbuf[PACKAGE_MIN_SIZE] = {0};
    fileid = GetMidID(p);
    offset = GetMidOffset(p);
    datalen = GetMidDatalen(p);
    //xil_printf("do %s, fileid = 0x%x, offset = %d.\r\n", __FUNCTION__, fileid, offset);
    // find info.
    BlockDataInfo *info = findInfo(&(gHandle.gBlockDataList), fileid);
    if(info == NULL){
    	xil_printf("do %s, not find fileid 0x%x.\r\n", __FUNCTION__, fileid);
        errmsg = axu_get_error_msg(A_MID_UID_NOT_FOUND);
        strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": uid is %u.", fileid);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_UID_NOT_FOUND, msgbuf, strlen(msgbuf), res);
        return PRET_ERROR;
    }
    if(info->flag == UPGRADE_ABORT){
    	xil_printf("do %s, upgrade has been aborted.\r\n", __FUNCTION__);
    	errmsg = axu_get_error_msg(A_UPGRADE_STATE_ERROR);
		strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": upgrade has been aborted.");
		make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_STATE_ERROR, msgbuf, strlen(msgbuf), res);
		return PRET_ERROR;
    }

    // check datalen.
    if((offset + datalen) > info->dataLen){
        errmsg = axu_get_error_msg(A_MID_LEN_ERROR);
        strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": o(%d) + l(%d) > tl(%d).", offset, datalen, info->dataLen);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_LEN_ERROR, msgbuf, strlen(msgbuf), res);
        return PRET_ERROR;
    }
    // restructure data.
    memcpy(&(info->data[offset]), &(p->data[TRANSPORT_MID_DATA_OFFSET]), datalen);
    info->recLen += datalen;
    make_response_ack(p, ACMD_BP_RES_ACK, 1, res);

    send_upgrade_progress(5+(info->recLen * 75/info->dataLen), "receiving"); // max progress is 80%.

    return PRET_OK;
}

#define TRANSPORT_FIN_ID_SIZE (4)
#define TRANSPORT_FIN_OFFSET_SIZE (4)
#define TRANSPORT_FIN_LEN_SIZE (4)
#define TRANSPORT_FIN_DATA_OFFSET (TRANSPORT_FIN_ID_SIZE+TRANSPORT_FIN_OFFSET_SIZE+TRANSPORT_FIN_LEN_SIZE)
#define GetFinID(pack)        (*((u32*)(&pack->data[0])))
#define GetFinOffset(pack)    (*((u32*)(&pack->data[0+4])))
#define GetFinDatalen(pack)   (*((u32*)(&pack->data[0+4+4])))
static PRET doTransportFin(A_Package *p, A_Package *res)
{
    u32 fileid = 0, datalen = 0;
    u32 offset = 0;

    char *errmsg = NULL;
    char msgbuf[PACKAGE_MIN_SIZE] = {0};
    fileid = GetFinID(p);
    offset = GetFinOffset(p);
    datalen = GetFinDatalen(p);

    xil_printf("do %s, fileid = 0x%x, offset = %d.\r\n", __FUNCTION__, fileid, offset);
    // find info.
    BlockDataInfo *info = findInfo(&(gHandle.gBlockDataList), fileid);
    xil_printf("info = %p\r\n", info);
    if(info == NULL){
    	xil_printf("do %s, not find fileid.\r\n", __FUNCTION__);
        errmsg = axu_get_error_msg(A_MID_UID_NOT_FOUND);
        strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": uid is %u.", fileid);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_UID_NOT_FOUND, msgbuf, strlen(msgbuf), res);
        return PRET_ERROR;
    }

    // check datalen.
    if((offset + datalen) > info->dataLen){
        errmsg = axu_get_error_msg(A_MID_LEN_ERROR);
        strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": o(%d) + l(%d) > tl(%d).", offset, datalen, info->dataLen);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_LEN_ERROR, msgbuf, strlen(msgbuf), res);
        return PRET_ERROR;
    }
    // restructure data.
    xil_printf("offset = %d, datalen = %d\r\n", offset, datalen);
    memcpy(info->data+offset, p->data+(TRANSPORT_MID_DATA_OFFSET), datalen);
    info->recLen += datalen;

    // recheck full file content.
    u32 chk = checksum(info->data, info->dataLen);
    xil_printf("checksum = 0x%x, info->chk = 0x%x\r\n", chk, info->checksum);
//    for(u32 i = 0; i  < info->dataLen; i++)
//    {
//        xil_printf("d[%d] = 0x%02x\r\n", i, info->data[i]);
//    }
    if(chk != info->checksum){
    	xil_printf("do %s, full file checksum check failed.\r\n", __FUNCTION__);
        errmsg = axu_get_error_msg(A_FIN_CHECKSUM_ERROR);
        strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": checksum not match.");
        make_response_error(p, ACMD_BP_RES_ERR, A_FIN_CHECKSUM_ERROR, msgbuf, strlen(msgbuf), res);
        return PRET_ERROR;
    }
    xil_printf("do %s, full file checksum check passed.\r\n", __FUNCTION__);
    if(info->flag == UPGRADE_ABORT){
    	xil_printf("do %s, upgrade has been aborted.\r\n", __FUNCTION__);
		errmsg = axu_get_error_msg(A_UPGRADE_STATE_ERROR);
		strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": upgrade has been aborted.");
		make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_STATE_ERROR, msgbuf, strlen(msgbuf), res);
		return PRET_ERROR;
	}
    info->flag = UPGRADE_RECV_FIN;
    xil_printf("do %s, received fileid = 0x%x.\r\n", __FUNCTION__, fileid);

    make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
    send_upgrade_progress(80, "receive finished");
    return PRET_OK;
}

#define UPGRADE_ID_SIZE (4)
#define GetUpgradeID(pack)        (*((u32*)(&pack->data[0])))
static PRET doUpgradeStart(A_Package *p, A_Package *res)
{
    u32 fileid = 0;
    u32 bootAddr = gHandle.gEnv.bootaddr;
    u32 upgradeAddr = 0x0, writeLen = 0x0;
    FPartation fp_golden, fp_img1, fp_img2;

    char *errmsg = NULL;
    char msgbuf[PACKAGE_MIN_SIZE] = {0};
    fileid = GetUpgradeID(p);
    xil_printf("do %s, fileid = 0x%x.\r\n", __FUNCTION__, fileid);

    // find info.
    BlockDataInfo *info = findInfo(&(gHandle.gBlockDataList), fileid);
    if(info == NULL){
    	xil_printf("do %s, not find fileid .\r\n", __FUNCTION__);
        errmsg = axu_get_error_msg(A_MID_UID_NOT_FOUND);
        strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": uid is %u.", fileid);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_UID_NOT_FOUND, msgbuf, strlen(msgbuf), res);
        return PRET_ERROR;
    }
    if(info->flag == UPGRADE_ABORT){
    	xil_printf("do %s, upgrade has been aborted.\r\n", __FUNCTION__);
		errmsg = axu_get_error_msg(A_UPGRADE_STATE_ERROR);
		strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": upgrade has been aborted.");
		make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_STATE_ERROR, msgbuf, strlen(msgbuf), res);
		return PRET_ERROR;
	}

    if(info->flag != UPGRADE_RECV_FIN){
    	xil_printf("do %s, file not receive finish.\r\n", __FUNCTION__);
    	errmsg = axu_get_error_msg(A_UPGRADE_STATE_ERROR);
    	strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": state is %d, can't do upgrade.", info->flag);
    	make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_STATE_ERROR, msgbuf, strlen(msgbuf), res);
    	return PRET_ERROR;
    }

    // find write add
    FM_GetPartation(FM_GOLDEN_PNAME, &fp_golden);
    FM_GetPartation(FM_IMAGE1_PNAME, &fp_img1);
    FM_GetPartation(FM_IMAGE2_PNAME, &fp_img2);


    if(info->eDataUsage == BD_USE_UPGRADE_GOLDEN){
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
    //FM_Print(&fp_golden);
    //FM_Print(&fp_img1);
    //FM_Print(&fp_img2);

    if(info->flag == UPGRADE_ABORT){
		errmsg = axu_get_error_msg(A_UPGRADE_STATE_ERROR);
		strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": upgrade has been aborted.");
		make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_STATE_ERROR, msgbuf, strlen(msgbuf), res);
		return PRET_ERROR;
	}

    info->flag = UPGRADE_ERASEING_FLASH;

    xil_printf("do %s, start erase flash 0x%x, len = 0x%x.\r\n", __FUNCTION__, upgradeAddr, writeLen);
    // flash erase.
    if(0 != FlashErase(gHandle.gFlashInstancePtr, upgradeAddr, writeLen, CmdBfr)){
        xil_printf("Upgrade: flash erase error, addr:0x%x, size:0x%x.\r\n", upgradeAddr, writeLen);
        errmsg = axu_get_error_msg(A_UPGRADE_FLASH_ERASE_ERROR);
        make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_FLASH_ERASE_ERROR, errmsg, strlen(errmsg), res);
        return PRET_ERROR;
    }
    send_upgrade_progress(88, "flash erase finished");

    // check upgrade abort flag.
	if(info->flag == UPGRADE_ABORT){
		errmsg = axu_get_error_msg(A_UPGRADE_STATE_ERROR);
		strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": upgrade has been aborted.");
		make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_STATE_ERROR, msgbuf, strlen(msgbuf), res);
		return PRET_ERROR;
	}

	info->flag = UPGRADE_WRITEING_FLASH;
	xil_printf("do %s, start write flash 0x%x, len = 0x%x.\r\n", __FUNCTION__, upgradeAddr, info->dataLen);
    // write to flash
    if(0 != FlashWrite(gHandle.gFlashInstancePtr, upgradeAddr, info->dataLen, info->data)){
        xil_printf("Upgrade: flash write error, addr:0x%x, size:0x%x.\r\n", upgradeAddr, info->dataLen);
        errmsg = axu_get_error_msg(A_UPGRADE_FLASH_WRITE_ERROR);
        make_response_error(p, ACMD_BP_RES_ERR, A_UPGRADE_FLASH_WRITE_ERROR, errmsg, strlen(errmsg), res);
        return PRET_ERROR;
    }
    info->flag = UPGRADE_WRITE_FLASH_FIN;
    send_upgrade_progress(95, "flash write finished");

    gHandle.gEnv.bootaddr = upgradeAddr;
    gHandle.gEnv.update_flag = UPGRADE_REBOOT;
    if(XST_SUCCESS == env_update(&(gHandle.gEnv))){
    	xil_printf("do %s, env_upgrade finished.\r\n", __FUNCTION__);
    	make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
    	msg_pool_txsend(gHandle.gMsgPoolIns, res, axu_package_len(res));
    }else{
    	xil_printf("do %s, env_upgrade failed.\r\n", __FUNCTION__);
    	errmsg = axu_get_error_msg(A_ENV_UPDATE_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_ENV_UPDATE_ERROR, errmsg, strlen(errmsg), res);
		return PRET_ERROR;
    }
    xil_printf("do %s, upgrade come to reset.\r\n", __FUNCTION__);
    sleep(1);
    //u32 addr = GetRealAddr(gHandle.gFlashInstancePtr, upgradeAddr);

    doMultiBoot(0);
    return PRET_NORES;
}

static PRET doUpgradeAbort(A_Package *p, A_Package *res)
{
    u32 fileid = 0;

    char *errmsg = NULL;
    char msgbuf[PACKAGE_MIN_SIZE] = {0};
    fileid = GetUpgradeID(p);
    xil_printf("do %s, fileid = 0x%x.\r\n", __FUNCTION__, fileid);
    // find info.
    BlockDataInfo *info = findInfo(&(gHandle.gBlockDataList), fileid);
    if(info == NULL){
    	xil_printf("do %s, not find fileid.\r\n", __FUNCTION__);
        errmsg = axu_get_error_msg(A_MID_UID_NOT_FOUND);
        strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": uid is %u.", fileid);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_UID_NOT_FOUND, msgbuf, strlen(msgbuf), res);
        return PRET_ERROR;
    }
    if(info->flag < UPGRADE_WRITEING_FLASH){
        info->flag = UPGRADE_ABORT;
        xil_printf("do %s, upgrade abort.\r\n", __FUNCTION__);
        make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
    }else {
        errmsg = axu_get_error_msg(A_UPGRADE_ABORT_ERROR);
        strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": state is %d.", info->flag);
        make_response_error(p, ACMD_BP_RES_ERR, A_MID_UID_NOT_FOUND, msgbuf, strlen(msgbuf), res);
    }

    return PRET_OK;
}

static PRET doGetVersion(A_Package *p, A_Package *res)
{
    int offset = 0;
    axu_package_init(res, p, ACMD_BP_RES_ACK);
    axu_set_data(res, offset, &gVersion.H, 1);
    offset += 1;
    axu_set_data(res, offset, &gVersion.M, 1);
    offset += 1;
    axu_set_data(res, offset, &gVersion.F, 1);
    offset += 1;
    axu_set_data(res, offset, &gVersion.D, 1);
    offset += 1;
    axu_finish_package(res);
    xil_printf("do: %s\r\n", __FUNCTION__);
    return PRET_OK;
}

static PRET doReset(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
	msg_pool_txsend(gHandle.gMsgPoolIns, res, axu_package_len(res));

	GoMultiBoot(0);
    return PRET_OK;
}

static PRET doGetRandom(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	make_response_random(p, ACMD_BP_RES_ACK,res);

    return PRET_OK;
}
static PRET doGetBoeSN(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	make_response_sn(p, ACMD_BP_RES_ACK,res);
    return PRET_OK;
}
static PRET doGetBoeMAC(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	make_response_mac(p, ACMD_BP_RES_ACK,res);
    return PRET_OK;
}

static PRET doReadPhyReg(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	char *errmsg = NULL;
	u32 reg = *(u32*)(p->data);
	u16 val = 0;
	if(XST_SUCCESS == emac_reg_read(reg, &val)){
		make_response_phy_read(p, ACMD_BP_RES_ACK, val, res);
	}else{
		errmsg = axu_get_error_msg(A_PHY_READ_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_PHY_READ_ERROR, errmsg, strlen(errmsg), res);
	}

    return PRET_OK;
}

static PRET doReadPhyShdReg(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	char *errmsg = NULL;
	u32 reg = *(u32*)(p->data);
	u16 shadow = *(u16*)(p->data+4);
	u16 val = 0;
	if(XST_SUCCESS == emac_shadow_reg_read(reg, shadow, &val)){
		xil_printf("phy shd read [0x%x][0x%02x] = [0x%02x]\r\n", reg, shadow, val);
		make_response_phy_read(p, ACMD_BP_RES_ACK, val, res);
	}else{
		errmsg = axu_get_error_msg(A_PHY_READ_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_PHY_READ_ERROR, errmsg, strlen(errmsg), res);
	}

    return PRET_OK;
}

static PRET doWritePhyShdReg(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	char *errmsg = NULL;
	u32 reg = *(u32*)(p->data);
	u16 shadow = *(u16*)(p->data+4);
	u16 val = *(u16*)(p->data+4+2);
	if(XST_SUCCESS == emac_shadow_reg_write(reg, shadow, val)){
		make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
	}else{
		errmsg = axu_get_error_msg(A_PHY_WRITE_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_PHY_WRITE_ERROR, errmsg, strlen(errmsg), res);
	}

    return PRET_OK;
}

static PRET doWriteReg(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	u32 reg = *(u32*)(p->data);
	u32 val = *(u32*)(p->data+4);
	Xil_Out32(reg, val);
	make_response_ack(p, ACMD_BP_RES_ACK, 1, res);

    return PRET_OK;
}

static PRET doReadReg(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);

	u32 reg = *(u32*)(p->data);
	u32 val = 0;
	val = Xil_In32(reg);
	make_response_reg_read(p, ACMD_BP_RES_ACK, val, res);

    return PRET_OK;
}
// vcc make random
unsigned char g_random[32] = {0};
void hash_random_get()
{
	u8  temp = 0;
	u8  sum = 0;
	u32 val = 0;
	u32 i = 0;
	u32 j = 0;

	u32 reg = 0xffa50804;//vcc reg
	
	memset(g_random, 0, 32);
	for(i = 0; i < 32; i ++)
	{
		temp = 0;
		sum = 0;
		for(j = 0; j < 8; j++)
		{
			val = Xil_In32(reg);
			temp = val & 0x1;
			sum = (temp << (7 - j)) | sum;
			usleep(500);//mast,or error
		}		
		g_random[i] = sum & 0xff;
	}
}

static PRET doReadRandomReg(A_Package *p, A_Package *res)
{
	unsigned char random_error[32];
	unsigned char random_fail[32];
	
	memset(random_error, 0x00, sizeof(random_error));
	memset(random_fail, 0xff, sizeof(random_fail));
	
	if((0 == strcmp(random_error, g_random)) || (0 == strcmp(random_fail, g_random)))
	{		
		int status = at508_get_random(g_random, sizeof(g_random));		
		if(status != PRET_OK)
		{
			for(int i = 0; i < 8; i++)
			{
				u32 r = genrandom();
				memcpy(&g_random[i*4], &r, sizeof(r));
			}
		}
	}
	make_response_random_read(p, ACMD_BP_RES_ACK, g_random, res);

    return PRET_OK;

}

#if 0
static PRET doGetHWVer(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	make_response_version(p, ACMD_BP_RES_ACK, gVersion.H, res);
    return PRET_OK;
}
static PRET doGetFWVer(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	make_response_version(p, ACMD_BP_RES_ACK, gVersion.F, res);
    return PRET_OK;
}
static PRET doGetAXUVer(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	make_response_version(p, ACMD_BP_RES_ACK, gAXUVersion, res);
    return PRET_OK;
}
#endif
static PRET doSetBoeSN(A_Package *p, A_Package *res)
{
	char *errmsg = NULL;
	memcpy(gHandle.gEnv.board_sn, p->data, 20);

	xil_printf("do: %s\r\n", __FUNCTION__);
	if(env_update(&(gHandle.gEnv)) == 0) {
		make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
	}else{
		errmsg = axu_get_error_msg(A_ENV_UPDATE_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_ENV_UPDATE_ERROR, errmsg, strlen(errmsg), res);
	}
    return PRET_OK;
}

static PRET doSetBoeMAC(A_Package *p, A_Package *res)
{
	char *errmsg = NULL;
	memcpy(gHandle.gEnv.board_mac, p->data, 6);

	xil_printf("do: %s\r\n", __FUNCTION__);
	if(env_update(&(gHandle.gEnv)) == 0) {
		make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
	}else{
		errmsg = axu_get_error_msg(A_ENV_UPDATE_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_ENV_UPDATE_ERROR, errmsg, strlen(errmsg), res);
	}
    return PRET_OK;
}

#if 0 // remove unbind at 2018-6-20
static PRET doUnBind(A_Package *p, A_Package *res)
{
	memset(gHandle.gEnv.bindID, 0, sizeof(gHandle.gEnv.bindID));
	memset(gHandle.gEnv.bindAccount, 0, sizeof(gHandle.gEnv.bindAccount));
	gHandle.gBindId = 0;
	gHandle.gBindAccount = 0;
	env_update(&gHandle.gEnv);
	make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
    return PRET_OK;
}
#endif
#if 0 // remove check bind at 2018-6-20
static PRET doCheckBind(A_Package *p, A_Package *res)
{
	char *errmsg = NULL;
	if(memcmp(gHandle.gEnv.bindID, gEmpty256, sizeof(gEmpty256)) != 0 &&
		memcmp(gHandle.gEnv.bindID, p->data, sizeof(gHandle.gEnv.bindID)) == 0)
	{
		gHandle.gBindId = 1;
	}
	else
	{
		gHandle.gBindId = 0;
	}
	if(memcmp(gHandle.gEnv.bindAccount, gEmpty256, sizeof(gEmpty256)) != 0 &&
		memcmp(gHandle.gEnv.bindAccount, p->data+sizeof(gEmpty256), sizeof(gHandle.gEnv.bindAccount)) == 0)
	{
		gHandle.gBindAccount = 1;
	}
	else
	{
		gHandle.gBindAccount = 0;
	}
	if(gHandle.gBindId == 1 && gHandle.gBindAccount == 1){
		make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
	}else{
		errmsg = axu_get_error_msg(A_BINDID_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_BINDID_ERROR, errmsg, strlen(errmsg), res);
		return PRET_ERROR;
	}

    return PRET_OK;
}
#endif

static PRET doGetBindAccount(A_Package *p, A_Package *res)
{
	int offset = 0;
	axu_package_init(res, p, ACMD_BP_RES_ACK);
	axu_set_data(res, offset, gHandle.gEnv.bindAccount, sizeof(gHandle.gEnv.bindAccount));

	offset += sizeof(gHandle.gEnv.bindAccount);
	axu_finish_package(res);

	xil_printf("do: %s\r\n", __FUNCTION__);
	return PRET_OK;
}

#if 0 // remove bindid 2018-6-20
static PRET doBindID(A_Package *p, A_Package *res)
{
	char *errmsg = NULL;
	char msgbuf[PACKAGE_MIN_SIZE] = {0};
	if(memcmp(gEmpty256, gHandle.gEnv.bindID, sizeof(gEmpty256)) == 0){
		memcpy(gHandle.gEnv.bindID, p->data, sizeof(gEmpty256));
		env_update(&gHandle.gEnv);
		gHandle.gBindId = 1;
		make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
	}else{
		errmsg = axu_get_error_msg(A_BINDID_ERROR);
		strcat(msgbuf, errmsg);
		sprintf(msgbuf+strlen(errmsg), ": id has bound.");
		make_response_error(p, ACMD_BP_RES_ERR, A_BINDID_ERROR, msgbuf, strlen(msgbuf), res);
		return PRET_ERROR;
	}

    return PRET_OK;
}
#endif

static PRET doBindAccount(A_Package *p, A_Package *res)
{
	char *errmsg = NULL;

	xil_printf("do: %s\r\n", __FUNCTION__);
	//for(int i = 0; i< sizeof(gHandle.gEnv.bindAccount); i++)
	//{
	//	xil_printf("ra[%d] = 0x%02x\r\n", i, p->data[i]);
	//}
	memcpy(gHandle.gEnv.bindAccount, p->data, sizeof(gHandle.gEnv.bindAccount));
	if(env_update(&(gHandle.gEnv)) == 0) {
		gHandle.gBindAccount = 1;
		make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
	}else{
		errmsg = axu_get_error_msg(A_ENV_UPDATE_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_ENV_UPDATE_ERROR, errmsg, strlen(errmsg), res);
	}
    return PRET_OK;
}

static PRET doHWSign(A_Package *p, A_Package *res)
{
	int datalen = p->header.body_length;
	u8 *sdata = p->data;
	char *errmsg = NULL;
	if(datalen != 32){
		errmsg = axu_get_error_msg(A_HWSIGN_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_HWSIGN_ERROR, errmsg, strlen(errmsg), res);
		return PRET_ERROR;
	}
	u8 hash[32];
	u8 signature[64];
	memcpy(hash, sdata, 32);
	int status = at508_sign(hash, signature, sizeof(signature));
	if(status != PRET_OK){
		errmsg = axu_get_error_msg(A_HWSIGN_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_HWSIGN_ERROR, errmsg, strlen(errmsg), res);
		return PRET_ERROR;
	}

	int plen = sizeof(signature) + sizeof(A_Package);
	A_Package *sp = (A_Package*)malloc(plen);
	sp->header.body_length = sizeof(signature);
	axu_package_init(sp, p, ACMD_BP_RES_ACK);
	axu_set_data(sp, 0, signature, sizeof(signature));
	axu_finish_package(sp);
	msg_pool_txsend(gHandle.gMsgPoolIns, sp, axu_package_len(sp));
	axu_package_free(sp);

	xil_printf("do: %s\r\n", __FUNCTION__);

    return PRET_NORES;
}

static PRET doGenKey(A_Package *p, A_Package *res)
{
	char *errmsg = NULL;
	u8 pubkey[64];

	int status = at508_genkey(pubkey, sizeof(pubkey));
	if(status != PRET_OK){
		errmsg = axu_get_error_msg(A_GENKEY_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_GENKEY_ERROR, errmsg, strlen(errmsg), res);
		return PRET_ERROR;
	}

	int plen = sizeof(pubkey) + sizeof(A_Package);
	A_Package *sp = (A_Package*)malloc(plen);
	sp->header.body_length = sizeof(pubkey);
	axu_package_init(sp, p, ACMD_BP_RES_ACK);
	axu_set_data(sp, 0, pubkey, sizeof(pubkey));
	axu_finish_package(sp);
	msg_pool_txsend(gHandle.gMsgPoolIns, sp, axu_package_len(sp));
	axu_package_free(sp);

	xil_printf("do: %s\r\n", __FUNCTION__);

    return PRET_NORES;
}

static PRET doGetPubkey(A_Package *p, A_Package *res)
{
	char *errmsg = NULL;
	u8 pubkey[64];

	int status = at508_getpubkey(pubkey, sizeof(pubkey));
	if(status != PRET_OK){
		errmsg = axu_get_error_msg(A_GETPUBKEY_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_GETPUBKEY_ERROR, errmsg, strlen(errmsg), res);
		return PRET_ERROR;
	}

	int plen = sizeof(pubkey) + sizeof(A_Package);
	A_Package *sp = (A_Package*)malloc(plen);
	sp->header.body_length = sizeof(pubkey);
	axu_package_init(sp, p, ACMD_BP_RES_ACK);
	axu_set_data(sp, 0, pubkey, sizeof(pubkey));
	axu_finish_package(sp);
	msg_pool_txsend(gHandle.gMsgPoolIns, sp, axu_package_len(sp));
	axu_package_free(sp);

	xil_printf("do: %s\r\n", __FUNCTION__);

    return PRET_NORES;
}
static PRET doLockPk(A_Package *p, A_Package *res)
{
	char *errmsg = NULL;

	int status = at508_lock_privatekey();
	if(status != PRET_OK){
		errmsg = axu_get_error_msg(A_LOCK_PK_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_LOCK_PK_ERROR, errmsg, strlen(errmsg), res);
		return PRET_ERROR;
	}
	make_response_ack(p, ACMD_BP_RES_ACK, 1, res);

	xil_printf("do: %s\r\n", __FUNCTION__);

    return PRET_OK;
}

static PRET doHWVerify(A_Package *p, A_Package *res)
{
	int datalen = p->header.body_length;
	u8 *sdata = p->data;
	char *errmsg = NULL;
	if(datalen < 160){
		errmsg = axu_get_error_msg(A_HWVERIFY_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_HWVERIFY_ERROR, errmsg, strlen(errmsg), res);
		return PRET_ERROR;
	}
	u8 hash[32];
	u8 signature[64];
	u8 pubkey[64];
	memcpy(hash, sdata, 32);
	memcpy(signature, sdata+32, 64);
	memcpy(pubkey, sdata+32+64, 64);
	u8 is_verified = 0;
	int status = at508_verify(hash, signature, pubkey, &is_verified);
	if(status != PRET_OK || (is_verified == 0)){
		errmsg = axu_get_error_msg(A_HWVERIFY_ERROR);
		make_response_error(p, ACMD_BP_RES_ERR, A_HWVERIFY_ERROR, errmsg, strlen(errmsg), res);
		return PRET_ERROR;
	}

	make_response_ack(p, ACMD_BP_RES_ACK, 1, res);
	xil_printf("do: %s\r\n", __FUNCTION__);
	return PRET_OK;
}

Processor gCmdProcess[ACMD_END] = {
	    [ACMD_PB_GET_VERSION_INFO] 	= {ACMD_PB_GET_VERSION_INFO, NULL, doGetVersion},
	    [ACMD_PB_TRANSPORT_START] 	= {ACMD_PB_TRANSPORT_START, NULL, doTransportStart},
	    [ACMD_PB_TRANSPORT_MIDDLE] 	= {ACMD_PB_TRANSPORT_MIDDLE, NULL, doTransportMid},
	    [ACMD_PB_TRANSPORT_FINISH] 	= {ACMD_PB_TRANSPORT_FINISH, NULL, doTransportFin},
	    [ACMD_PB_UPGRADE_START] 	= {ACMD_PB_UPGRADE_START, NULL, doUpgradeStart},
	    [ACMD_PB_UPGRADE_ABORT] 	= {ACMD_PB_UPGRADE_ABORT, NULL, doUpgradeAbort},
	    [ACMD_PB_RESET] 			= {ACMD_PB_RESET, NULL, doReset},
	    [ACMD_PB_GET_RANDOM] 		= {ACMD_PB_GET_RANDOM, NULL, doGetRandom},
	    [ACMD_PB_GET_SN] 		= {ACMD_PB_GET_SN, NULL, doGetBoeSN},
#if 0 // not used.
	    [ACMD_PB_GET_HW_VER] 		= {ACMD_PB_GET_HW_VER, NULL, doGetHWVer},
	    [ACMD_PB_GET_FW_VER] 		= {ACMD_PB_GET_FW_VER, NULL, doGetFWVer},
	    [ACMD_PB_GET_AXU_VER] 		= {ACMD_PB_GET_AXU_VER, NULL, doGetAXUVer},
#endif
	    [ACMD_PB_SET_SN] 		= {ACMD_PB_SET_SN, NULL, doSetBoeSN},

		[ACMD_PB_BIND_ACCOUNT] 		= {ACMD_PB_BIND_ACCOUNT, NULL, doBindAccount},
		[ACMD_PB_GET_ACCOUNT] 		= {ACMD_PB_GET_ACCOUNT, NULL, doGetBindAccount},
		[ACMD_PB_HW_SIGN] 			= {ACMD_PB_HW_SIGN, NULL, doHWSign},
#if 0 // not used.
		[ACMD_PB_CHECK_BIND] 		= {ACMD_PB_CHECK_BIND, NULL, doCheckBind},
		[ACMD_PB_BIND_ID] 			= {ACMD_PB_BIND_ID, NULL, doBindID},
		[ACMD_PB_UNBIND] 			= {ACMD_PB_UNBIND, NULL, doUnBind},
#endif
		[ACMD_PB_GENKEY]			= {ACMD_PB_GENKEY, NULL, doGenKey},
		[ACMD_PB_GET_PUBKEY]		= {ACMD_PB_GET_PUBKEY, NULL, doGetPubkey},
		[ACMD_PB_LOCK_PRIKEY]		= {ACMD_PB_LOCK_PRIKEY, NULL, doLockPk},
		[ACMD_PB_VERIFY]		= {ACMD_PB_VERIFY, NULL, doHWVerify},
		[ACMD_PB_SET_MAC]		= {ACMD_PB_SET_MAC, NULL, doSetBoeMAC},
		[ACMD_PB_GET_MAC]		= {ACMD_PB_GET_MAC, NULL, doGetBoeMAC},
		[ACMD_PB_PHY_READ]		= {ACMD_PB_PHY_READ, NULL, doReadPhyReg},
		[ACMD_PB_PHY_SHD_READ]		= {ACMD_PB_PHY_SHD_READ, NULL, doReadPhyShdReg},
		[ACMD_PB_PHY_SHD_WRITE]		= {ACMD_PB_PHY_SHD_WRITE, NULL, doWritePhyShdReg},
		[ACMD_PB_REG_WRITE]		= {ACMD_PB_REG_WRITE, NULL, doWriteReg},
		[ACMD_PB_REG_READ]		= {ACMD_PB_REG_READ, NULL, doReadReg},
		
		[ACMD_PB_REG_RANDOM]		= {ACMD_PB_REG_RANDOM, NULL, doReadRandomReg},

};

Processor* processor_get(ACmd acmd)
{
	if(acmd <= ACMD_START || acmd >= ACMD_END)
		return NULL;
	else
		return &gCmdProcess[acmd];
}


