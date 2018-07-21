/*
 * led.h
 *
 *  Created on: 2018年7月20日
 *      Author: luxq
 */

#ifndef SRC_LED_H_
#define SRC_LED_H_

#include <xil_types.h>

#define LED_ALL (0x0F)   /* there are four led  */
#define LED_1   (0x01)
#define LED_2   (0x02)
#define LED_3   (0x04)
#define LED_4   (0x08)

int ledInit(void);
int ledHigh(u8 led);
int ledLow(u8 led);

#endif /* SRC_LED_H_ */
