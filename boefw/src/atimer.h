/*
 * atimer.h
 *
 *  Created on: 2018年7月26日
 *      Author: luxq
 */

#ifndef SRC_ATIMER_H_
#define SRC_ATIMER_H_

#include "xil_types.h"
#define MAX_TIMER_FUNC_NUM (10)
// if TimerFunc return none zero, the timer function will continue;
// if TimerFunc return 0, the timer function will remove from timer call list.
typedef int (*TimerFunc)(void *userdata);
int atimer_init(void);
int atimer_release();
void atimer_reset();
u64 atimer_gettm();
// if register functions number is reach MAX_TIMER_FUNC_NUM,
// the function will return failed.
int atimer_register_timer(TimerFunc func, u64 internal_ms, void *userdata);

#endif /* SRC_ATIMER_H_ */
