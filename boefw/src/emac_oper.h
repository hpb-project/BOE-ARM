/*
 * emac_init.h
 *
 *  Created on: 2018年8月14日
 *      Author: luxq
 */

#ifndef SRC_EMAC_OPER_H_
#define SRC_EMAC_OPER_H_

int emac_init(void);
int emac_reg_read(u32 RegisterNum, u16 *PhyDataPtr);
int emac_shadow_reg_read(u32 RegisterNum, u16 shadow, u16 *PhyDataPtr);
int emac_shadow_reg_write(u32 RegisterNum, u16 shadow, u16 val);
#endif /* SRC_EMAC_OPER_H_ */
