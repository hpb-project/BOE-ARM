/*
 * env.h
 *
 *  Created on: Jun 4, 2018
 *      Author: luxq
 */

#ifndef SRC_ENV_H_
#define SRC_ENV_H_

#defined ENV_SIZE (0x20000)  // 128KB

typedef struct Env {
	uint32 start_magic;
	uint32 boot_address;

	uint8  reversed[];
	uint32 end_magic;
};


#endif /* SRC_ENV_H_ */
