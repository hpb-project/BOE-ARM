/*
 * env.c
 *
 *  Created on: Jun 4, 2018
 *      Author: luxq
 */
#include <stdio.h>
#include "flash_oper.h"
#include "flash_map.h"
#include "env.h"
#include "xil_printf.h"
#include "compiler.h"

#define START_MAGIC (0xA8)
#define END_MAGIC	(0xC5)
#define UNKNOWN_FLASH_SECTOR_SIZE (0x10000)  // 64KB
#define UNKNOWN_FLASH_SECTOR_MASK (0xFFFF0000)


#define ENV_DATA_SIZE (2048)

typedef struct EnvHandle {
    u32 start_addr;
    u32 partation_size;
    u32 end_addr;
    u32 current_addr;
    u32 flash_sector_size;	// erase unit.
    u32 flash_sector_mask;
    u32 bInit;
    XQspiPsu flashInstance;
    FPartation fp;
    EnvContent genv;
}EnvHandle;


static u8 CmdBfr[8];
static EnvHandle gHandle = {
    .bInit = 0,
};

static u32 checksum(u8 *pdata, u32 len)
{
    u32 cks = 0;
    for(u32 i = 0; i < len; i++)
    {
        cks += pdata[i];
    }
    return cks;
}

//
static int findValidEnv(EnvHandle *handle, u32 sAddress, EnvContent *genv)
{
    EnvContent lastEnv, tmp;
    u32 lastAddr = 0;
    int Status;
    do {
        Status = FlashRead(&(handle->flashInstance), sAddress, sizeof(EnvContent), CmdBfr, (u8*)&tmp);
        if(Status != XST_SUCCESS){
            printf("Flash read failed.\n");
            return XST_FAILURE;
        }
        if(tmp.start_magic == START_MAGIC &&
                tmp.end_magic == END_MAGIC)
        {
            u32 cks = checksum((u8*)&tmp, sizeof(EnvContent) - sizeof(u32)); // don't checksum envChecksum
            if(cks == tmp.envChecksum){
                if(lastAddr != 0) {
                    // check who is newer env.
                    if(tmp.seq_id > lastEnv.seq_id){
                        memcpy(&lastEnv, &tmp, sizeof(EnvContent));
                        lastAddr = sAddress;
                    }
                }else{
                    memcpy(&lastEnv, &tmp, sizeof(EnvContent));
                    lastAddr = sAddress;
                }
            }
        }
        sAddress += sizeof(EnvContent);
    }while(sAddress < handle->end_addr);

    if (lastAddr != 0) {
        printf("find valid env find, at 0x%x, env.boeid = %d.\n", lastAddr, lastEnv.boeid);
        handle->current_addr = lastAddr;
        memcpy(genv, &lastEnv, sizeof(EnvContent));
        return XST_SUCCESS;
    }
    printf("not find valid env.\n");
    return XST_FAILURE;
}

static int readEnv(EnvHandle *handle, EnvContent *env)
{
    int Status = FlashRead(&(handle->flashInstance), handle->current_addr, sizeof(EnvContent), CmdBfr, (u8*)env);
    if(Status != XST_SUCCESS){
        printf("Flash read failed.\n");
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}


static int writeEnv(EnvHandle *handle, EnvContent *env)
{
    env->seq_id++;

    u32 cks = checksum((u8*)env, sizeof(EnvContent) - sizeof(u32));
    env->envChecksum = cks;

    int Status = XST_SUCCESS;
    u32 Address = handle->current_addr + sizeof(EnvContent);

    u32 writeAddress = Address;
    if((Address%(handle->flash_sector_size) + sizeof(EnvContent)) >= handle->flash_sector_size) {
        u32 nextSector = 0;
        if((Address + sizeof(EnvContent)) >= handle->end_addr ) {
            // erase first sector.
            nextSector = handle->start_addr;
        }else {
            // erase next sector.
            nextSector = (Address + handle->flash_sector_size) & handle->flash_sector_mask;
        }
        Status = FlashErase(&handle->flashInstance, nextSector, handle->flash_sector_size, CmdBfr);
        printf("Flash Erase 0x%x.\n", nextSector);
        if(Status != XST_SUCCESS){
            xil_printf("Flash erase failed.\n");
            env->seq_id--;
            return XST_FAILURE;
        }
        writeAddress = nextSector;
    }
    Status = FlashWrite(&handle->flashInstance, writeAddress, sizeof(EnvContent), (u8*)env);
    printf("flash write env to 0x%x.\n", writeAddress);
    if(Status != XST_SUCCESS){
        xil_printf("Flash write failed.\n");
        env->seq_id--;
        return XST_FAILURE;
    }
    handle->current_addr = writeAddress;
    return XST_SUCCESS;
}

void initDefEnv(EnvHandle *handle, EnvContent *env)
{
    memset(env, 0x0, sizeof(EnvContent));
    env->start_magic = START_MAGIC;
    env->end_magic = END_MAGIC;

    FPartation fp;
    FM_GetPartation(FM_IMAGE1_PNAME, &fp);
    env->bootaddr = fp.pAddr;
}

int env_init(void)
{
    int Status = 0;

    if(0 == gHandle.bInit){
        Status = FlashInit(&gHandle.flashInstance);
        if (Status != XST_SUCCESS) {
            xil_printf("Flash Init failed.\r\n");
            return XST_FAILURE;
        }
        Status = FM_GetPartation(FM_ENV_PNAME, &(gHandle.fp));
        if(Status != XST_SUCCESS){
            xil_printf("Get partation info failed.\n");
            return XST_FAILURE;
        }
        FlashInfo fi;
        Status = FlashGetInfo(&fi);
        if(Status != XST_SUCCESS) {
            xil_printf("Get flashinfo failed.\r\n");
            gHandle.flash_sector_size = UNKNOWN_FLASH_SECTOR_SIZE;
            gHandle.flash_sector_mask = UNKNOWN_FLASH_SECTOR_MASK;
        }else{
            gHandle.flash_sector_size = fi.SectSize;
            gHandle.flash_sector_mask = fi.SectMask;
        }
        gHandle.start_addr = gHandle.fp.pAddr; // align sector size.
        gHandle.partation_size = gHandle.fp.pSize;
        gHandle.end_addr = gHandle.start_addr + gHandle.partation_size;
        gHandle.current_addr = gHandle.start_addr;

        if (findValidEnv(&gHandle, gHandle.start_addr, &gHandle.genv) != XST_SUCCESS)
        {
            // 1. erase all env partation
            Status = FlashErase(&(gHandle.flashInstance), gHandle.start_addr, gHandle.flash_sector_size, CmdBfr);
            printf("flash erase 0x%x, len = %d.\n", gHandle.start_addr, gHandle.flash_sector_size);
            if(Status != XST_SUCCESS){
                xil_printf("Flash erase failed.\n");
                return XST_FAILURE;
            }
            // 2. write default env.
            initDefEnv(&gHandle, &gHandle.genv);
            Status = writeEnv(&gHandle, &gHandle.genv);
            if(Status != XST_SUCCESS){
                xil_printf("write default env failed.\n");
                return XST_FAILURE;
            }
        }
        gHandle.bInit = 1;
    }
    return XST_SUCCESS;
}

int env_get(EnvContent *env)
{
    if(gHandle.bInit){
        //memcpy(env, &gHandle.genv, sizeof(EnvContent));
        return readEnv(&gHandle, env);
    }
    return XST_FAILURE;
}

int env_update(EnvContent *env)
{
    if(gHandle.bInit
            && env->start_magic == START_MAGIC
            && env->end_magic == END_MAGIC)
    {
        if(writeEnv(&gHandle, env) == XST_SUCCESS){
            memcpy(&gHandle.genv, env, sizeof(EnvContent));
            return XST_SUCCESS;
        }
    }
    return XST_FAILURE;
}

static void buildCheck()
{
    BUILD_CHECK_SIZE_EQUAL(sizeof(EnvContent), ENV_DATA_SIZE);
}
