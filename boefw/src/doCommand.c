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
	uint32_t r = genrandom();
	xil_printf("random = %d\r\n", r);
    axu_package_init(p, req, cmd);
    axu_set_data(p, 0, (u8*)&r, sizeof(r));
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
#define GetHWVersion(pack)				(pack->data[1+4+4+4])
#define GetFWVersion(pack)				(pack->data[1+4+4+4+1])
#define GetAXUVersion(pack)				(pack->data[1+4+4+4+1+1])

static int checkVersion(A_Package *p)
{
	u8 nhwVer = GetHWVersion(p);
	u8 nbfVer = GetFWVersion(p);
	u8 naxuVer = GetAXUVersion(p);
	if(vMajor(nhwVer) == vMajor(gHWVersion)
			&& vMinor(nhwVer) >= vMinor(gHWVersion))
	{
		if(nbfVer >= gBFVersion && naxuVer >= gAXUVersion){
			if((nhwVer == gHWVersion) && (nbfVer == gBFVersion) && (naxuVer == gAXUVersion)){
				return 1;
			}
			return 0;
		}
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
    return PRET_OK;
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
    axu_set_data(res, offset, &gHWVersion, 1);
    offset += 1;
    axu_set_data(res, offset, &gBFVersion, 1);
    offset += 1;
    axu_set_data(res, offset, &gAXUVersion, 1);
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

	GoReset();
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
static PRET doGetHWVer(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	make_response_version(p, ACMD_BP_RES_ACK, gHWVersion, res);
    return PRET_OK;
}
static PRET doGetFWVer(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	make_response_version(p, ACMD_BP_RES_ACK, gBFVersion, res);
    return PRET_OK;
}
static PRET doGetAXUVer(A_Package *p, A_Package *res)
{
	xil_printf("do: %s\r\n", __FUNCTION__);
	make_response_version(p, ACMD_BP_RES_ACK, gAXUVersion, res);
    return PRET_OK;
}

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

static void hwsign(u8 *src, int srclen, u8 *sign, int slen)
{
	int len = srclen > slen ? slen : srclen;
	memset(sign, 0x0, slen);
	for(int i = 0; i < len; i++)
	{
		sign[i] = src[i]<<2;
	}
	return;
}
static PRET doHWSign(A_Package *p, A_Package *res)
{
	int datalen = p->header.body_length;
	u8 *sdata = p->data;

	// todo: change checksum to other function.
	u8 result[65];
	hwsign(sdata, datalen, result, sizeof(result));

	int plen = sizeof(result) + sizeof(A_Package);
	A_Package *sp = (A_Package*)malloc(plen);
	sp->header.body_length = sizeof(result);
	axu_package_init(sp, p, ACMD_BP_RES_ACK);
	axu_set_data(sp, 0, (u8*)&result, sizeof(result));
	axu_finish_package(sp);
	msg_pool_txsend(gHandle.gMsgPoolIns, sp, axu_package_len(sp));
	axu_package_free(sp);

	xil_printf("do: %s\r\n", __FUNCTION__);

    return PRET_NORES;
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
	    [ACMD_PB_GET_HW_VER] 		= {ACMD_PB_GET_HW_VER, NULL, doGetHWVer},
	    [ACMD_PB_GET_FW_VER] 		= {ACMD_PB_GET_FW_VER, NULL, doGetFWVer},
	    [ACMD_PB_GET_AXU_VER] 		= {ACMD_PB_GET_AXU_VER, NULL, doGetAXUVer},
	    [ACMD_PB_SET_SN] 		= {ACMD_PB_SET_SN, NULL, doSetBoeSN},

		[ACMD_PB_BIND_ACCOUNT] 		= {ACMD_PB_BIND_ACCOUNT, NULL, doBindAccount},
		[ACMD_PB_GET_BINDINFO] 		= {ACMD_PB_GET_BINDINFO, NULL, doGetBindAccount},
		[ACMD_PB_HW_SIGN] 			= {ACMD_PB_HW_SIGN, NULL, doHWSign},
//		[ACMD_PB_CHECK_BIND] 		= {ACMD_PB_CHECK_BIND, NULL, doCheckBind},
//		[ACMD_PB_BIND_ID] 			= {ACMD_PB_BIND_ID, NULL, doBindID},
//		[ACMD_PB_UNBIND] 			= {ACMD_PB_UNBIND, NULL, doUnBind},

		[ACMD_PB_GENKEY]			= {ACMD_PB_GENKEY, NULL, NULL},
		[ACMD_PB_GET_PUBKEY]		= {ACMD_PB_GET_PUBKEY, NULL, NULL},
		[ACMD_PB_LOCK_PRIKEY]		= {ACMD_PB_LOCK_PRIKEY, NULL, NULL},
		[ACMD_PB_VERIFY]		= {ACMD_PB_VERIFY, NULL, NULL},
		[ACMD_PB_SET_MAC]		= {ACMD_PB_SET_MAC, NULL, NULL},
		[ACMD_PB_GET_MAC]		= {ACMD_PB_GET_MAC, NULL, NULL},

};

Processor* processor_get(ACmd acmd)
{
	if(acmd <= ACMD_START || acmd >= ACMD_END)
		return NULL;
	else
		return &gCmdProcess[acmd];
}


