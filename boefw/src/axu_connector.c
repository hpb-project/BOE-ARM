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
#include <stdlib.h>
#include "axu_connector.h"
#include "community.h"
#include "compiler.h"

static uint32_t  g_sequence_id = 0;
#define MAX_PACKAGE_LENGTH (2048+sizeof(A_Package))

#define fetch_axu_package_sequence() (g_sequence_id++)

static void axu_init_package(A_Package *pack)
{
    pack->header.magic_aacc = 0xaacc;
    pack->header.package_id = fetch_axu_package_sequence();
    pack->header.body_length = 0;
    pack->header.magic_ccaa = 0xccaa;
    pack->header.acmd = ACMD_END;
    pack->checksum = 0;

}
static void finish_package(A_Package *pack)
{
    if(pack->header.body_length > 0)
    {
        pack->checksum = checksum(pack->data, pack->header.body_length);
    }
}


static int package_send(A_Package *package)
{
    if(package == NULL)
        return -1;
    int ret = 0; //fifo_send((uint8_t *)package, sizeof(A_Package)+package->header.body_length);
    if(ret > 0)
        return 0;
    return ret;
}

static int package_receive(A_Package *package)
{
    A_Package *p = NULL;
    uint8_t data[MAX_PACKAGE_LENGTH];
    int ret = 0;//fifo_read(data, sizeof(data));
    p = (A_Package*) data;
    if (p->header.magic_aacc == 0xAACC &&
            p->header.magic_ccaa == 0xCCAA)
    {
        if(p->header.body_length > 0 && 
            p->header.body_length < (MAX_PACKAGE_LENGTH-sizeof(A_Package)))
        {
            uint32_t s_chk = p->checksum;
            uint32_t d_chk = checksum(p->data, p->header.body_length);
            if(s_chk == d_chk)
            {
                memcpy(package, p, sizeof(A_Package)+p->header.body_length);
                return 0;
            }
        }
        else if (p->header.body_length == 0)
        {
            memcpy(package, p, sizeof(A_Package));
            return 0;
        }
    }
    return -3;
}

static A_Package *axu_get_response(ACmd cmd)
{
    A_Package package;
    axu_init_package(&package);
    package.header.acmd = cmd;
    finish_package(&package);

    package_send(&package);

    A_Package *r_pack = (A_Package *)malloc(MAX_PACKAGE_LENGTH);
    memset(r_pack, 0x0, MAX_PACKAGE_LENGTH);
    if(package_receive(r_pack))
        return r_pack;
    else
    {
        if(r_pack) 
            free(r_pack);
        return NULL;
    }
}

uint32_t axu_get_random()
{
    uint32_t random = 0;
    A_Package *r_pack = axu_get_response(ACMD_PB_GET_RANDOM);
    if(r_pack != NULL 
            && (r_pack->header.acmd == ACMD_BP_RES_RANDOM))
    {
        random = *(uint32_t*)(r_pack->data);
        free(r_pack);
    }
    else
    {
        random = rand();
    }
    return random;
}





int axu_get_boeid(uint32_t *p_id)
{
    int ret = -1;
    A_Package *r_pack = axu_get_response(ACMD_PB_GET_RANDOM);
    if(r_pack != NULL 
            && (r_pack->header.acmd == ACMD_BP_RES_RANDOM))
    {
        *p_id = *(uint32_t*)(r_pack->data);
        free(r_pack);
        ret = 0;
    }
    return ret;
}

int axu_set_boeid(uint32_t boeid)
{
    int ret = -1;
    A_Package *package = (A_Package*)malloc(sizeof(A_Package) + 4);
    axu_init_package(package);
    package->header.acmd = ACMD_PB_SET_BOEID;
    finish_package(package);
    package_send(package);
    A_Package *r_pack = (A_Package*)malloc(sizeof(A_Package) + 100);
    if(r_pack)
    {
        memset(r_pack, 0x0, sizeof(A_Package) + 100);
        if(package_receive(r_pack) 
                && r_pack->header.acmd == ACMD_BP_RES_ACK)
        {
            ret = 0;
        }
        free(r_pack);
    }
    return ret;
}


int axu_update(void)
{
    // 1. 获取对方的版本号信息
    // 2. 查找可用的升级文件
    // 3. 下载升级文件
    // 4. 传输升级文件
    // 5. 下发开始升级指令
    // 6. 等待升级进度和结果
    return 0;
}

int axu_update_abort(void)
{
    int ret = -1;
    A_Package *package = (A_Package*)malloc(sizeof(A_Package));
    axu_init_package(package);
    package->header.acmd = ACMD_PB_UPGRADE_ABORT;
    finish_package(package);
    package_send(package);
    A_Package *r_pack = (A_Package*)malloc(sizeof(A_Package) + 200);
    //Todo: error check.
    if(r_pack)
    {
        memset(r_pack, 0x0, sizeof(A_Package) + 100);
        if(package_receive(r_pack) 
                && r_pack->header.acmd == ACMD_BP_RES_ACK)
        {
            ret = 0;
        }
        free(r_pack);
    }
    return ret;
}

A_Package* apackage_new(int len)
{
	return (A_Package*)malloc(len);
}

int apackage_free(A_Package *pack)
{
	if(pack != NULL)
		free(pack);
	return 0;
}

static void buildCheck()
{
    BUILD_CHECK_SIZE_EQUAL(sizeof(A_Package_Header), 10);
}
