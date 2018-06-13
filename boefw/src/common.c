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


#include "common.h"
#include <stdio.h>
#include <stdlib.h>

GlobalHandle gHandle = {.bRun = 1, };
uint32_t checksum(uint8_t *data, uint32_t len)
{
    uint32_t chk = 0;
    if(data != NULL && len > 0)
    {
        for(uint32_t i = 0;i < len; i++)
        {
            chk += data[i];
        }
    }
    return chk;
}

uint32_t genrandom(void)
{
    return (uint32_t)random();
}

BlockDataInfo *findInfo(BlockDataInfo *head, u32 uniqueId)
{
    BlockDataInfo *p = head;
    while(p!=NULL){
        if(p->uniqueId == uniqueId)
            return p;
        else
            p = p->next;
    };
    return p;
}
int addInfo(BlockDataInfo *head, BlockDataInfo *nInfo)
{
    BlockDataInfo *p = head;
    while(p!=NULL){
            p = p->next;
    };
    p->next = nInfo;
    return 0;
}

int removeInfo(BlockDataInfo *head, BlockDataInfo *rInfo)
{
    BlockDataInfo *last = head;
    BlockDataInfo *p = NULL;
    while(last != NULL && last->next != NULL)
    {
        p = last->next;
        if(p->uniqueId == rInfo->uniqueId)
        {
            last->next = p->next;
            free(p);
            return 0;
        }
        last = p;
    };
    return 1;
}

int removeInfoByID(BlockDataInfo *head, u32 uniqueId)
{
    BlockDataInfo *last = head;
    BlockDataInfo *p = NULL;
    while(last != NULL && last->next != NULL)
    {
        p = last->next;
        if(p->uniqueId == uniqueId)
        {
            last->next = p->next;
            free(p);
            return 0;
        }
        last = p;
    };
    return 1;
}
