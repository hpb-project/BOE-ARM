/*
 * version.c
 *
 *  Created on: Jun 6, 2018
 *      Author: luxq
 */

#include "version.h"


#if 1
/*
 *  Release note:
 *  	2019.5.15  version release.
 *  	fpga version 1.12.1, add new random hash function with fid=0x3
 */
TVersion gVersion = {
		.H = 0x11,
		.M = 0,
		.F = 1,
		.D = 0
};
#endif

#if 0
/*
 *  Release note:
 *  	2019.1.10  version release.
 *  	fpga version 1.9.1, add new random hash function with fid=0x3
 */
TVersion gVersion = {
		.H = 0x11,
		.M = 0,
		.F = 0,
		.D = 2
};
#endif



#if 0
/*
 *  Release note:
 *  	2018.9.3  version release.
 *  	fpga version 1.8
 *  	axu add register r/w interface.
 */
TVersion gVersion = {
		.H = 0x11,
		.M = 0,
		.F = 0,
		.D = 1
};
#endif
