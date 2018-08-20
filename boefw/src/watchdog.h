/*
 * watchdog.h
 *
 *  Created on: 2018年8月20日
 *      Author: luxq
 */

#ifndef SRC_WATCHDOG_H_
#define SRC_WATCHDOG_H_

int watch_dog_init();
int watch_dog_start(int cnt_ms);
int watch_dog_restart();
int watch_dog_expired();

#endif /* SRC_WATCHDOG_H_ */
