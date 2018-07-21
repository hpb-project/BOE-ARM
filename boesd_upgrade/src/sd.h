/*
 * sd.h
 *
 *  Created on: 2018年7月19日
 *      Author: luxq
 */

#ifndef SRC_SD_H_
#define SRC_SD_H_

#define SD_OPEN_FAILED  (-1)
#define SD_CLOSE_FAILED (-2)
#define SD_FAILED       (-3)
#define SD_SUCCESS      (0)

int sdInit(void);
int sdRead(char *filename, u8 *buf, u64 *rlen);

#endif /* SRC_SD_H_ */
