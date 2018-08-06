#include <stdio.h>
#include "platform.h"
#include "xplatform_info.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xparameters.h"
#include "xil_types.h"
#include "xstatus.h"
#include "xqspipsu.h"
#include "sleep.h"
#include "flash_map.h"
#include "flash_oper.h"
#include "env.h"
#include "sd.h"
#include "JSON.h"
#include "led.h"

#define IMAGE_BUF_SIZE (128*1024*1024) // 128MB
#define ENV_BUF_SIZE   (20*1024*1024) // 20MB

typedef struct ImageHeader{
    uint8_t usage;
    uint8_t vendor[3];
    uint32_t chk;
    uint32_t len;
    uint8_t hw;
    uint8_t fw;
    uint8_t axu;
 }ImageHeader;

typedef struct UpgradeImgInfo{
	char imageName[100];
	u8   *imgBuf;
	u32  imgLen;
	u32  pAddr;        	// partation start addr
	u32  pLen;      	// partation len
}UpgradeImgInfo;

typedef struct UpgradeInfo{
	char version[50];
	UpgradeImgInfo img[10];
	int  imgNum;
}UpgradeInfo;


typedef struct Handle{
	FlashInfo fi;
	EnvHandle *eHandle;
	UpgradeInfo info;
}Handle;

static Handle gHandle;

static char *DefaultImageName = "upgrade.bin";
static char *DefaultEnvName   = "env.txt";

static u8 *gImgBuf = NULL;

static u8 *gEnvBuf = NULL;
static u64 gEnvLen = 0;

static const char *dCurrentVersion = "hpb1.0";
/*
{
	"version":"hpb1.0",
	"upgradelist":[
		{
		"imageName":"golden.bin",
		"partationOffset":"0x1000000",
		"partationLen":"0x2000000",
		},
	]
}

 */



static int htoi(char *s)
{
    int i;
    u32 n = 0;

    if (s[0] == '0' && (s[1]=='x' || s[1]=='X'))
    {
        i = 2;
    }
    else
    {
        i = 0;
    }
    for (; (s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'z') || (s[i] >='A' && s[i] <= 'Z');++i)
    {
        if (tolower(s[i]) > '9')
        {
            n = 16 * n + (10 + tolower(s[i]) - 'a');
        }
        else
        {
            n = 16 * n + (tolower(s[i]) - '0');
        }
    }
    return n;

}


static int parseJson(char *envBuf, u64 bufLen, UpgradeInfo *info)
{
	JSON *root = JSON_Parse(envBuf);
	int status = XST_FAILURE;
	if(root!=NULL)
	{
		JSON *jversion = JSON_GetObjectItem(root,"version");
		if((jversion != NULL) && (jversion->type == JSON_String)){
			if(strcmp(jversion->valuestring, dCurrentVersion) != 0){
				xil_printf("env version not match, parse failed.\r\n");
				return XST_FAILURE;
			}
			strcpy(info->version, jversion->valuestring);

			JSON *upgradelist = JSON_GetObjectItem(root, "upgradelist");
			if((upgradelist != NULL) && (upgradelist->type == JSON_Array)){
				int cnt = JSON_GetArraySize(upgradelist);
				for(int i = 0; i < cnt; i++){
					JSON *uitem = JSON_GetArrayItem(upgradelist, i);
					UpgradeImgInfo fi;
					memset(&fi, 0x0, sizeof(fi));
					{
						// parse upgrade img info.
						JSON *iname = JSON_GetObjectItem(uitem,"imageName");
						if((iname != NULL) && (iname->type == JSON_String)){
							strncpy(fi.imageName, iname->valuestring, sizeof(fi.imageName));
						}else{
							continue;
						}

						JSON *partationOffset = JSON_GetObjectItem(uitem,"partationOffset");
						if(partationOffset != NULL && partationOffset->type == JSON_String){
							fi.pAddr = htoi(partationOffset->valuestring);
						}else{
							continue;
						}
						JSON *partationLen = JSON_GetObjectItem(uitem,"partationLen");
						if(partationLen != NULL && partationLen->type == JSON_String){
							fi.pLen = htoi(partationLen->valuestring);
						}else{
							continue;
						}
						memcpy(&(info->img[info->imgNum]), &fi, sizeof(fi));
						info->imgNum++;
					}
				}
			}
			status = XST_SUCCESS;

		}
		else
		{
			xil_printf("Have no version info.\r\n");

			status = XST_FAILURE;
		}
		JSON_Delete(root);
	}
	return status;
}
uint32_t checksum(uint8_t *data, uint32_t len)
{
    uint32_t chk = 0;
    if(data != NULL && len > 0)
    {
        for(uint32_t i = 0;i < len; i++)
        {
            chk += data[i];
        }
    }
    return chk;
}

static u8 CmdBfr[8];
static int upgradeBin(Handle *handle, UpgradeImgInfo *info)
{
	XQspiPsu *QspiPsuPtr = &handle->eHandle->flashInstance;
	u32 pAddr = info->pAddr;
	u32 pLen  = info->pLen;
	int status = XST_SUCCESS;
	ImageHeader *header = (ImageHeader*)info->imgBuf;
	u8 *pdata = info->imgBuf + sizeof(ImageHeader);
	u64 imglen = info->imgLen - sizeof(ImageHeader);
	u32 chk = checksum(pdata, imglen);
	if(chk != header->chk){
		xil_printf("Image checksum not match.\r\n");
		return XST_FAILURE;
	}
	u8 *buf = (u8*)malloc(imglen);
	memset(buf, 0x0, imglen);

	status = FlashErase(QspiPsuPtr, pAddr, pLen, CmdBfr);
	if(status != XST_SUCCESS){
		xil_printf("Flash erase 0x%x, len 0x%x failed.\r\n", pAddr, pLen);
		return status;
	}
	status = FlashWrite(QspiPsuPtr, pAddr, imglen, pdata);
	if(status != XST_SUCCESS){
		xil_printf("Flash write 0x%x, len 0x%x failed.\r\n", pAddr, imglen);
		return status;
	}
	// recheck
	status = FlashRead(QspiPsuPtr, pAddr, imglen, CmdBfr, buf);
	if(status != XST_SUCCESS){
		xil_printf("Flash read 0x%x, len 0x%x failed.\r\n", pAddr, imglen);
		return status;
	}
	for(u32 i = 0; i < imglen; i++){
		if(buf[i] != pdata[i]){
			xil_printf("Flash write and read not match, failed.\r\n");
			return XST_FAILURE;
		}
	}
	free(buf);

	return XST_SUCCESS;
}

static int doSDUpgrade(Handle *handle)
{
	int status;

    gEnvLen = ENV_BUF_SIZE;
    status = sdRead(DefaultEnvName, gEnvBuf, &gEnvLen);
    if(status == SD_SUCCESS){
    	// parse env.txt
		parseJson((char*)gEnvBuf, gEnvLen, &handle->info);
    }
    if(handle->info.imgNum > 0){
    	ledHigh(LED_1 | LED_2);
    	for(int i = 0; i < handle->info.imgNum; i++){
    		UpgradeImgInfo *ui = &handle->info.img[i];
    		ui->imgBuf = gImgBuf;
    		ui->imgLen = IMAGE_BUF_SIZE;
    		memset(ui->imgBuf, 0x0, ui->imgLen);
    		status = sdRead(ui->imageName, ui->imgBuf, &ui->imgLen);
    		ledHigh(LED_1 | LED_2 | LED_3);
    	    if(status == SD_SUCCESS){
    	    	if(XST_SUCCESS != upgradeBin(handle, ui)){
    	    		xil_printf("upgrade %s failed.\r\n", ui->imageName);
    	    		return XST_FAILURE;
    	    	}else{
    	    		xil_printf("Upgrade %s success.\r\n", ui->imageName);
    	    	}
    	    }else{
    	    	xil_printf("read %s failed.\r\n", ui->imageName);
				return XST_FAILURE;
    	    }
    	}
    	return XST_SUCCESS;

    }else{
    	// if have no env.txt, then only support upgrade FW1 and set bootaddr to FW1.
    	UpgradeImgInfo ui;
    	FPartation fp;
		EnvContent env;

		FM_GetPartation(FM_IMAGE1_PNAME, &fp);
		memset(&ui, 0x0, sizeof(ui));
    	strcpy(ui.imageName, DefaultImageName);
    	ui.imgBuf    = gImgBuf;
    	ui.imgLen  = IMAGE_BUF_SIZE;
    	ui.pAddr = fp.pAddr;
		ui.pLen  = fp.pSize;

    	status = env_get(&env);
    	if(status != XST_SUCCESS){
    		xil_printf("env get failed.\r\n");
    		return XST_FAILURE;
    	}
    	ledHigh(LED_1 | LED_2);
    	memset(ui.imgBuf, 0x0, ui.imgLen);
    	status = sdRead(ui.imageName, ui.imgBuf, &ui.imgLen);
    	if(status != SD_SUCCESS){
    		xil_printf("Not find default image %s\r\n", ui.imageName);
    		return XST_FAILURE;
    	}
    	ledHigh(LED_1 | LED_2 | LED_3);
    	xil_printf("Find default image %s \r\n", ui.imageName);
    	if(XST_SUCCESS == upgradeBin(handle, &ui)){
    		env.bootaddr = ui.pAddr;
			status = env_update(&env);
			if(status == XST_SUCCESS){
				xil_printf("Upgrade %s success.\r\n", ui.imageName);
				return XST_SUCCESS;
			}else{
				xil_printf("update env failed.\r\n");
				return XST_FAILURE;
			}
		}else{
			xil_printf("upgrade %s failed.\r\n", ui.imageName);
			return XST_FAILURE;
		}
    }

    return 0;
}

int init(Handle *handle)
{
	int status = 0;

	memset(handle, 0x0, sizeof(Handle));
	//1. init flash and env.
	status = env_init();
	if(status != XST_SUCCESS){
		printf("env_init failed.\n");
		return -1;
	}

	// init sd
	status = sdInit();
	if(status != XST_SUCCESS){
		xil_printf("sd card init failed.\r\n");
		return -1;
	}

	status = ledInit();
	if(status != XST_SUCCESS){
		xil_printf("led init failed.\r\n");
		return -1;
	}

	handle->eHandle = env_get_handle();
	FlashGetInfo(&handle->fi);

	gImgBuf = (u8*)malloc(IMAGE_BUF_SIZE);
	gEnvBuf = (u8*)malloc(ENV_BUF_SIZE);
	if(gImgBuf == NULL || gEnvBuf == NULL)
	{
		xil_printf("Malloc memory failed.\r\n");
		return XST_FAILURE;
	}

	ledHigh(LED_1); //

	return XST_SUCCESS;
}

int led_upgrade_success()
{
	u8 led = 0xf;
	while(1){
		ledHigh(led);
		sleep(1);
		ledLow(led);
		usleep(500000);
	}
	return 0;
}

int led_upgrade_failed()
{
	u8 led = 0xf;
	while(1){
		ledHigh(led);
		usleep(200000);
		ledLow(led);
		usleep(200000);
	}
	return 0;
}

int main()
{

    init_platform();
    xil_printf("------------------- Enter SD Upgrade --------------\r\n");
#if 0
    extern int runtest();
    runtest();

#else
    if(XST_SUCCESS == init(&gHandle)){
    	if(XST_SUCCESS == doSDUpgrade(&gHandle)){
    		xil_printf("sd upgrade success.\r\n");
    		led_upgrade_success();
    	}
    }

    // run to here is upgrade failed.
    xil_printf("sd upgrade failed.\r\n");
    led_upgrade_failed();
#endif
    cleanup_platform();
    return 0;
}
