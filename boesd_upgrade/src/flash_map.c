// Last Update:2018-06-06 09:17:55
/**
 * @file flash_map.c
 * @brief 
 * @author luxueqian
 * @version 0.1.00
 * @date 2018-06-06
 */
#include "flash_map.h"
#include "xstatus.h"
#include <string.h>
#include "compiler.h"

//#define TEST_HPB
#ifdef TEST_HPB
#else  // test boe
#define FLASH_BASEADDR 	(0)
#define FLASH_SIZE     	(0x4000000)    // 64MB
#define FLASH_END 		(FLASH_BASEADDR + FLASH_SIZE)
#define GOLDEN_OFFSET 	(FLASH_BASEADDR)
#define GOLDEN_SIZE   	(0x200000)		// 2MB
#define ENV_OFFSET		(GOLDEN_OFFSET + GOLDEN_SIZE)
#define ENV_SIZE		(0x200000)		// 2MB
#define REVERSED_OFFSET (ENV_OFFSET + ENV_SIZE)
#define REVERSED_SIZE   (0x400000)		// 4MB
#define IMAGE_1_OFFSET  (REVERSED_OFFSET + REVERSED_SIZE)
#define IMAGE_1_SIZE    (0x1B00000)		// 28MB
#define IMAGE_2_OFFSET  (IMAGE_1_OFFSET + IMAGE_1_SIZE)
#define IMAGE_2_SIZE    (0x1B00000)		// 28MB
#define FLASH_USE_END   (ENV_OFFSET+ENV_SIZE)
#endif

static FPartation gDefPartationList[] = {
    {0, FM_GOLDEN_PNAME, GOLDEN_OFFSET, GOLDEN_SIZE},
	{1, FM_ENV_PNAME, ENV_OFFSET, ENV_SIZE},
    {2, FM_IMAGE1_PNAME, IMAGE_1_OFFSET, IMAGE_1_SIZE},
    {3, FM_IMAGE2_PNAME, IMAGE_2_OFFSET, IMAGE_2_SIZE},

};

int FM_Print(FPartation *p)
{
    xil_printf("p->pId = %d.\r\n", p->pId);
    xil_printf("p->pName = %s.\r\n", p->pName);
    xil_printf("p->pAddr = 0x%x.\r\n", p->pAddr);
    xil_printf("p->pSize = 0x%x.\r\n", p->pSize);
    return 0;
}

int FM_GetPartation(char *pName, FPartation *partation)
{
    int i = 0;
    for(i = 0; i < sizeof(gDefPartationList)/sizeof(FPartation); i++)
    {
        FPartation *fp = &(gDefPartationList[i]);
        if(strcmp(fp->pName, pName) == 0){
            xil_printf("copy fpinfo %s.\r\n", fp->pName);
            FM_Print(fp);
            memcpy(partation, fp, sizeof(FPartation));
            return XST_SUCCESS;
        }
    }
    return XST_FAILURE;
}

int FM_RemovePartation(char *pName)
{
    return XST_FAILURE;
}

int FM_AddPartation(FPartation *nPartation)
{
    return XST_FAILURE;
}

static void buildCheck()
{
    BUILD_CHECK_SIZE_ALIGN(GOLDEN_OFFSET,0x8000);   // 32KB align
    BUILD_CHECK_SIZE_ALIGN(GOLDEN_SIZE,0x10000);		// 64KB align
    BUILD_CHECK_SIZE_ALIGN(ENV_OFFSET,0x10000);
    BUILD_CHECK_SIZE_ALIGN(ENV_SIZE,0x10000);
    BUILD_CHECK_SIZE_ALIGN(IMAGE_1_OFFSET,0x8000);
    BUILD_CHECK_SIZE_ALIGN(IMAGE_1_SIZE,0x1000);
    BUILD_CHECK_SIZE_ALIGN(IMAGE_2_OFFSET,0x8000);
    BUILD_CHECK_SIZE_ALIGN(IMAGE_2_SIZE,0x1000);
}


#if (FLASH_USE_END > FLASH_END)
#error "FLASH_USE is bigger than FLASH_SIZE."
#endif
