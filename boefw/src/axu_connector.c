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
#define MAX_PACKAGE_LENGTH (PACKAGE_MAX_SIZE+sizeof(A_Package))

#define fetch_axu_package_sequence() (g_sequence_id++)

A_Package* axu_package_new(int len)
{
	//len += sizeof(A_Package);

    if(len > PACKAGE_MAX_SIZE)
        return NULL;

    A_Package * pack = (A_Package*)malloc(len + sizeof(A_Package));
    if(pack != NULL)
        pack->header.body_length = len;

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
        pack->header.q_or_r = 1;
    }else{
        pack->header.package_id = g_sequence_id++;
        pack->header.q_or_r = 0;
    }
    return ;
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

int axu_package_len(A_Package *pack)
{
	return pack->header.body_length + sizeof(A_Package);
}

const char *gErrMsg[] = {
		NULL,
		"unknown cmd",
		"magic err",
		"checksum err",
		"env update err",
		"usage err",
		"uid not find",
		"checksum_err",
		"datalen err",
		"datalen err",
		"upgrade has been aborted",
		"upgrade state err",
		"flash erase err",
		"flash write err",
		"upgrade abort err",
		"version err",
		"bind err",
		"hwsign err",
		"genkey err",
		"get pubkey err",
		"lock pk err",
		"hw verify err",
		"phy read err",
		"phy write err",
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
    BUILD_CHECK_SIZE_EQUAL(sizeof(A_Package_Header), 12);
}
