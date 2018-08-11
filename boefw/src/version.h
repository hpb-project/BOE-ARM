/*
 * version.h
 *
 *  Created on: Jun 6, 2018
 *      Author: luxq
 */

#ifndef SRC_VERSION_H_
#define SRC_VERSION_H_

#include <xil_types.h>
typedef struct TVersion{
	u8 H; // hardware version. 0xab
	u8 M; // match to ghpb version.
	u8 F; // function version
	u8 D; // debug version
}TVersion;
extern TVersion gVersion;

#endif /* SRC_VERSION_H_ */
