/*
 * env.h
 *
 *  Created on: Jun 4, 2018
 *      Author: luxq
 */

#ifndef SRC_ENV_H_
#define SRC_ENV_H_

#include <stdio.h>
#include "xil_types.h"
#pragma pack(push)
#pragma pack(1)

typedef struct EnvContent {
    u8  start_magic; 	// 0xA8
    u8  update_flag; 	// update flag.
    u8  reversed_1[2];

    u64 seq_id;			// env id, inc one per time.


    u32 bootaddr;		// for multiboot.
    u32 boeid;			// BOEID for the board.
    u8  bindAccount[32];// bind account address for the board.
    u8  reversed_3[32]; // old bind id.

    u8  reversed_2[1956];

    u8  reversed_0[3];
    u8  end_magic; 		// 0xC5;

    u32 envChecksum;  	// checksum above data.
}EnvContent;
#pragma pack(pop)

int env_init(void);
int env_get(EnvContent *env);
int env_update(EnvContent *env);

#endif /* SRC_ENV_H_ */
