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


#ifndef COMMUNITY_H
#define COMMUNITY_H
#include <xil_types.h>
#include "axu_connector.h"

typedef void*	MsgPoolHandle;

int msg_pool_init(MsgPoolHandle *handle);
int msg_pool_release(MsgPoolHandle *handle);
/*
 * fetch msg from msg pool with timeout, unit is ms.
 */
int msg_pool_fetch(MsgPoolHandle handle, A_Package **pack, u32 timeout_ms);
int msg_pool_txsend(MsgPoolHandle handle, A_Package *pack, int bytelen);

#endif  /*COMMUNITY_H*/
