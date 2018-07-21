// Last Update:2018-06-06 09:17:28
/**
 * @file flash_map.h
 * @brief 
 * @author luxueqian
 * @version 0.1.00
 * @date 2018-06-06
 */

#ifndef FLASH_MAP_H
#define FLASH_MAP_H

#include <stdio.h>
#include "xil_types.h"

#define FM_GOLDEN_PNAME "GoldenP"
#define FM_ENV_PNAME "EnvP"
#define FM_IMAGE1_PNAME "Image_1P"
#define FM_IMAGE2_PNAME "Image_2P"
/*
 * warning: partation name is unique.
 */
typedef struct FPartation {
	u8	pId;			// partation id.
	char pName[20];		// partation name.
	u32 pAddr;			// partation base address
	u32 pSize;			// partation size.
}FPartation;

int FM_GetPartation(char *pName, FPartation *partation); // get partation info by name.
int FM_RemovePartation(char *pName);					 // remove partation by name.
int FM_AddPartation(FPartation *nPartation);			 // add new partation.

#endif  /*FLASH_MAP_H*/
