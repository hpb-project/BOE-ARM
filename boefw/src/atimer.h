/*
 * atimer.h
 *
 *  Created on: 2018年7月26日
 *      Author: luxq
 */

#ifndef SRC_ATIMER_H_
#define SRC_ATIMER_H_

#include "xil_types.h"
int atimer_init(void);
int atimer_release();
void atimer_reset();
u64 atimer_gettm();

#endif /* SRC_ATIMER_H_ */
