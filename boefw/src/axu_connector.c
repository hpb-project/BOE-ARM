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

static uint16_t  g_sequence_id = 0;
#define MAX_PACKAGE_LENGTH (2048+sizeof(A_Package))

#define fetch_axu_package_sequence() (g_sequence_id++)

A_Package* axu_package_new(int len)
{
    if(len < PACKAGE_MIN_SIZE)
        len = PACKAGE_MIN_SIZE;
    if(len > PACKAGE_MAX_SIZE)
        return NULL;

    A_Package * pack = (A_Package*)malloc(len);
    if(pack != NULL)
        pack->header.body_length = len - sizeof(A_Package_Header);

    return pack;
}

int axu_package_free(A_Package *pack)
{
	if(pack != NULL)
		free(pack);
	return 0;
}

void axu_package_init(A_Package *pack, A_Package* req, ACmd cmd)
{
    pack->header.magic_aacc = 0xaacc;
    pack->header.magic_ccaa = 0xccaa;
    pack->header.acmd = cmd;
    pack->checksum = 0;
    memset(pack->data, 0, pack->header.body_length);

    if(req){
        // is a response package.
        pack->header.package_id = req->header.package_id;
    }else{
        pack->header.package_id = g_sequence_id++;
    }
}

int axu_set_data(A_Package *pack, int offset, u8 *data, int len)
{
    if((offset + len) > pack->header.body_length){
        return 1;
    }
    memcpy(pack->data + offset, data, len);
    return 0;
}

void axu_finish_package(A_Package *pack)
{
    pack->checksum = checksum(pack->data, pack->header.body_length);
}
const char *gErrMsg = {
    NULL,
    "Magic error",
    "Checksum error",
    NULL,
};
char *axu_get_error_msg(A_Error ecode)
{
    if(ecode > A_ERROR_START && ecode < A_ERROR_END)
        return gErrMsg[ecode];
    else
        return NULL;
}

static void buildCheck()
{
    BUILD_CHECK_SIZE_EQUAL(sizeof(A_Package_Header), 10);
}
