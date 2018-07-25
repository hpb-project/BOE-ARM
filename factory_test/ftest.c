// Last Update:2018-07-24 21:57:51
/**
 * @file rstest.c
 * @brief 
 * @author luxueqian
 * @version 0.1.00
 * @date 2018-07-12
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "rs.h"
#define MAGIC 0xcc11
#define REQ_SD_TEST      (0x1)
#define REQ_FLASH_TEST   (0x2)
#define REQ_DRAM_TEST    (0x3)
#define REQ_NET_TEST     (0x4)
#define REQ_CONNECT      (0x5)
#define REQ_RESULT       (0x6)
#define REQ_FINISH       (0x7)


#define RET_FAILED       (0x1)
#define RET_SUCCES       (0x2)

static uint32_t gpid = 0;

#define get_pid()    (gpid++)
typedef struct Package{
    uint16_t magic;
    uint16_t cmd;
    uint32_t pid;
    uint8_t  data[];
}Package;

static uint8_t tx[1024];
static uint8_t rx[1024];
static uint8_t result = 0;
int make_package_sd_test(Package *p)
{
    p->pid = get_pid();
    p->magic = MAGIC;
    p->cmd   = REQ_SD_TEST;
    return sizeof(Package);
}

int make_package_flash_test(Package *p)
{
    p->pid = get_pid();
    p->magic = MAGIC;
    p->cmd   = REQ_FLASH_TEST;
    return sizeof(Package);
}

int make_package_net_test(Package *p)
{
    p->pid = get_pid();
    p->magic = MAGIC;
    p->cmd   = REQ_NET_TEST;
    return sizeof(Package);
}

int make_package_dramtest(Package *p)
{
    p->pid = get_pid();
    p->magic = MAGIC;
    p->cmd   = REQ_DRAM_TEST;
    return sizeof(Package);
}

int make_package_conn(Package *p)
{
    p->pid = get_pid();
    p->magic = MAGIC;
    p->cmd   = REQ_CONNECT;
    return sizeof(Package);
}

int make_package_result(Package *p)
{
    p->pid = get_pid();
    p->magic = MAGIC;
    p->cmd   = REQ_RESULT;
    p->data[0] = result;
    return sizeof(Package) + 1;
}

int make_package_finish(Package *p)
{
    p->pid = get_pid();
    p->magic = MAGIC;
    p->cmd   = REQ_FINISH;
    return sizeof(Package);
}
typedef int (*MK_PKG)(Package*);

int _do(RSContext *rs, MK_PKG func, int timeout_s)
{
    Package *p = (Package*)tx;
    int wlen = 0, rlen = 0;
    memset(tx, 0x0, sizeof(tx));
    memset(rx, 0x0, sizeof(rx));
    wlen = func(p);
    if(RSWrite(rs, tx, wlen) < 0)
    {
        printf("RSWrite failed.\n");
        return -1;
    }
    int ret  = RSSelect(rs, timeout_s * 1000);
    if(ret > 0)
    {
        ret = RSRead(rs, rx, &rlen);
        if(ret < 0)
        {
            printf("read response failed.\n");
            return -1;
        }
        else
        {
            Package *rp = (Package*)rx;
            if(rp->pid == p->pid){
                if(rp->cmd == RET_SUCCES)
                    return 0;
                else
                    return 1;
            }
        }
    }
    else
    {
        printf("wait response timeout.\r\n");
        return -1;
    }
    return 0;
}

int dump_hex(char *input, int len)
{
    for(int i = 0; i < len; i++)
    {
        printf("[%d]=0x%02x\n", i, input[i]);
    }
    return 0;
}


int main(int argc, char *argv[])
{
    char *ethname;
    int type, ret;
    if(argc < 3)
    {
        printf("usage: %s ethname type\r\n", argv[0]);
        exit(-1);
    }
    ethname = argv[1];
    type = atoi(argv[2]);
    if(type == 0)
        type = 0xFF00;
    else if(type == 1)
        type = 0xFF01;
    else if(type == 2)
        type = 0xFF02;
    else 
        printf("unknown type(%d).\n", type);

    RSContext rs;
    ret = RSCreate(ethname, type, &rs);
    if(ret != 0)
    {
        printf("rscreate failed, please check ethname and use sudo.\r\n");
        return -1;
    }

    ret = _do(&rs, make_package_conn, 1);
    if(ret != 0)
    {
        printf("connect failed.\n");
        goto failed;
    }

    ret = _do(&rs, make_package_dramtest, 60);
    if(ret != 0)
    {
        printf("dramtest failed.\n");
        goto failed;
    }

    ret = _do(&rs, make_package_sd_test, 60);
    if(ret != 0)
    {
        printf("sdtest failed.\n");
        goto failed;
    }

    ret = _do(&rs, make_package_flash_test, 60);
    if(ret != 0)
    {
        printf("flashtest failed.\n");
        goto failed;
    }
    goto success;

failed:
    result = 1;
    _do(&rs, make_package_result, 1);
success:
    _do(&rs, make_package_finish, 1);

    RSRelease(&rs);
    return ret;
}
