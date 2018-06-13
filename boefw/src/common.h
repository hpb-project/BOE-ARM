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


#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include "xil_types.h"
#include "xstatus.h"
#include "xqspipsu.h"
#include "multiboot.h"
#include "sleep.h"
#include "flash_map.h"
#include "env.h"
#include "community.h"
#include "axu_connector.h"


typedef enum BlockDataUsage{
    BD_USE_START,
    BD_USE_UPGRADE_GOLDEN,
    BD_USE_UPGRADE_FW,
    BD_USE_END,
}BlockDataUsage;

typedef struct BlockDataInfo{
    BlockDataUsage eDataUsage; // data usage.
    u32 uniqueId;  // data unique id, used for restructure
    u32 checksum;  // data checksum.
    u32 dataLen;   // total length 
    u32 flag;      // update flag or something.
    struct BlockDataInfo *next; // link to next blockdata info.
    u8  data[];    // act data.
}BlockDataInfo;

typedef struct GlobalHandle {
    XQspiPsu 	gFlashInstance;
    EnvContent 	gEnv;
    BlockDataInfo gBlockDataList;
    MsgPoolHandle gMsgPoolIns;
    u8			bRun;
}GlobalHandle;

extern GlobalHandle gHandle;


BlockDataInfo *findInfo(BlockDataInfo *head, u32 uniqueId);
int addInfo(BlockDataInfo *head, BlockDataInfo *nInfo);
int removeInfo(BlockDataInfo *head, BlockDataInfo *rInfo);
int removeInfoByID(BlockDataInfo *head, u32 uniqueId);
uint32_t checksum(uint8_t *data, uint32_t len);
uint32_t genrandom();

#endif  /*COMMON_H*/
