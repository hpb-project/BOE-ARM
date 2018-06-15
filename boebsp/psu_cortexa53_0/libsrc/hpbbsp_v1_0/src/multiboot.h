/*
 * multiboot.h
 *
 *  Created on: Jun 4, 2018
 *      Author: luxq
 */

#ifndef SRC_MULTIBOOT_H_
#define SRC_MULTIBOOT_H_

// MultiBootValue: offset/0x8000
void GoMultiBoot(u32 MultiBootValue);
void GoReset(void);


#endif /* SRC_MULTIBOOT_H_ */
