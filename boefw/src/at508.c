/*
 * at508.c
 *
 *  Created on: 2018年8月6日
 *      Author: luxq
 */

#include "xil_types.h"
#include "atca_iface.h"
#include "atca_cfgs.h"
static const u16 PrivateKeySlot = 0;
ATCAIfaceCfg *gcfg = &cfg_ateccx08a_i2c_default;


static const u8 g_ecc_configdata[128] = {
    0x01, 0x23, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00,  0x04, 0x05, 0x06, 0x07, 0xEE, 0x00, 0x01, 0x00, // 0 - 15
    0xC0, 0x00, 0x55, 0x00, 0x8F, 0x20, 0xC4, 0x44, 0x87, 0x20, 0xC4, 0x44, 0x8F, 0x0F, 0x8F, 0x8F, // 16 - 31
    0x9F, 0x8F, 0x83, 0x64, 0xC4, 0x44, 0xC4, 0x44, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 47
    0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, // 63
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 79
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 95
    0x33, 0x00, 0x1C, 0x00, 0x13, 0x00, 0x1C, 0x00, 0x3C, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x33, 0x00, // 111
    0x1C, 0x00, 0x1C, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0x3C, 0x00  // 127
};
static void dump_hex_ln(u8* data, int len)
{
	for(int i = 0; i < len; i++){
		xil_printf("%02x ", data[i]);
	}
	xil_printf("\r\n");
}

int at508_init()
{
	ATCA_STATUS status;
	bool lockstate;
	status = atcab_init( gcfg );
	if(status != ATCA_SUCCESS){
		xil_printf("atcab init failed, status = %d.\r\n", status);
		return status;
	}

	status = atcab_is_locked(LOCK_ZONE_CONFIG, &lockstate);
	if (status != ATCA_SUCCESS){
		xil_printf("atcab is locked failed.\r\n");
		return status;
	}

	if (!lockstate)
	{
		status = atcab_write_config_zone(g_ecc_configdata);
		if (status != ATCA_SUCCESS){
			xil_printf("atcab write ecc config zone failed.\r\n");
			return status;
		}
		status = atcab_lock_config_zone();
		if (status != ATCA_SUCCESS){
			xil_printf("atcab lock config zone failed.\r\n");
			return status;
		}
	}

	return status;
}

int at508_release()
{
	atcab_release();
	return ATCA_SUCCESS;
}
int at508_get_sernum(u8 *sernum, int buflen)
{
	ATCA_STATUS status;
	if(buflen < ATCA_SERIAL_NUM_SIZE){
		xil_printf("serial number buffer length not enough\r\n");
		return -1;
	}
	status = atcab_read_serial_number( sernum );
	if(status != ATCA_SUCCESS){
		xil_printf("atcab read serial number failed.\r\n");
		return status;
	}

	return status;
}

int at508_genkey(u8 *pubkey, int buflen)
{
	ATCA_STATUS status;

	if(buflen < 64){
		xil_printf("pubkey buffer length not enough.\r\n");
		return -1;
	}
	status = atcab_genkey( 0, pubkey);
	if(status != ATCA_SUCCESS){
		xil_printf("atcab genkey failed.\r\n");
		return status;
	}

	return status;
}
// if we not get lock private key command, not call this function.
int at508_lock_privatekey()
{
	ATCA_STATUS status;
	bool lockstate;
	status = atcab_is_slot_locked(PrivateKeySlot, &lockstate);
	if (status != ATCA_SUCCESS){
		xil_printf("atcab is locked failed.\r\n");
		return status;
	}

	if (!lockstate)
	{
		status = atcab_lock_data_slot(PrivateKeySlot);
		if (status != ATCA_SUCCESS){
			xil_printf("atcab lock private key slot failed.\r\n");
		}
	}
	return status;
}

int at508_sign(u8 *hash, u8 *signature, int buflen)
{
	ATCA_STATUS status;
	if(buflen < 64){
		xil_printf("signature buffer length not enough.\r\n");
		return -1;
	}
	status = atcab_sign( PrivateKeySlot, hash, signature);
	if(status != ATCA_SUCCESS){
		xil_printf("atcab sign failed.\r\n");
	}

	return status;
}

int at508_verify(u8 *hash, u8 *signature, u8 *pubkey, u8 *is_verified)
{
	ATCA_STATUS status;
	bool bVerified = false;
	status = atcab_verify_extern(hash, signature, pubkey, &bVerified);
	if(status != ATCA_SUCCESS){
		xil_printf("atcab verify extern failed, status = %d.\r\n", status);
	}
	*is_verified = bVerified ? 1 : 0;
	return status;
}


int at508_get_random(u8 *random, int buflen)
{
	ATCA_STATUS status;
	if(buflen < 32){
		xil_printf("random buffer length not enough.\r\n");
		return -1;
	}

	status = atcab_random( random );
	if(status != ATCA_SUCCESS){
		xil_printf("atcab random failed.\r\n");
	}

	return status;
}


extern int at508_test(void)
{
	int status;
	uint8_t SerialNum[ATCA_SERIAL_NUM_SIZE];
	uint8_t SRandom[32];
	uint8_t SPubKey[64];
	uint8_t Signature[64];

	status = at508_init();
	if(status != ATCA_SUCCESS){
		xil_printf("at508 init failed.\r\n");
		return status;
	}

	status = at508_get_sernum(SerialNum, sizeof(SerialNum));
	if(status != ATCA_SUCCESS){
		xil_printf("at508 get sernum failed.\r\n");
		return status;
	}
	xil_printf("SerialNum:");
	dump_hex_ln(SerialNum, sizeof(SerialNum));

	status = at508_get_random(SRandom, sizeof(SRandom));
	if(status != ATCA_SUCCESS){
		xil_printf("at508 get random failed.\r\n");
		return status;
	}
	xil_printf("SRandom:");
	dump_hex_ln(SRandom, sizeof(SRandom));

	status = at508_genkey(SPubKey, sizeof(SPubKey));
	if(status != ATCA_SUCCESS){
		xil_printf("at508 genkey failed.\r\n");
		return status;
	}
	xil_printf("pubkey:");
	dump_hex_ln(SPubKey, sizeof(SPubKey));

	status = at508_sign(SRandom, Signature, sizeof(Signature));
	if(status != ATCA_SUCCESS){
		xil_printf("at508 sign failed.\r\n");
		return status;
	}
	xil_printf("signature:");
	dump_hex_ln(Signature, sizeof(Signature));

	u8 isVerify = 0;
	status = at508_verify(SRandom, Signature, SPubKey, &isVerify);
	if(status != ATCA_SUCCESS){
		xil_printf("at508 verify failed.\r\n");
		return status;
	}
	if(isVerify){
		xil_printf("verify ok.\r\n");
	}else{
		xil_printf("verify failed.\r\n");
	}


#if 0
	status = at508_ext_pkey_test(SRandom);
	if(status != ATCA_SUCCESS){
		xil_printf("at508 sign failed.\r\n");
		return status;
	}
#endif

	status = at508_release();
	if(status != ATCA_SUCCESS){
		xil_printf("at508 release failed.\r\n");
		return status;
	}
	return status;
}
