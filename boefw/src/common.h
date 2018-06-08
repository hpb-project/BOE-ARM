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

typedef struct GlobalHandle {
	XQspiPsu 	gFlashInstance;
	EnvContent 	gEnv;
	MsgPoolHandle gMsgPoolIns;
	u8			bRun;
}GlobalHandle;

extern GlobalHandle gHandle;

uint32_t checksum(uint8_t *data, uint32_t len);

#endif  /*COMMON_H*/
