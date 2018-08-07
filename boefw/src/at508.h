/*
 * at508.h
 *
 *  Created on: 2018年8月6日
 *      Author: luxq
 */

#ifndef SRC_AT508_H_
#define SRC_AT508_H_
#include "xil_types.h"
int at508_init(void);
int at508_release(void);
// sernum is 32bytes big-endian.
int at508_get_sernum(u8 *sernum, int buflen);
// the function will general a private key in atecc internal, and return 64bytes pubkey format in big-endian.
int at508_genkey(u8 *pubkey, int buflen);

int at508_getpubkey(u8 *pubkey, int buflen);
// the function will lock private key slot and can't genkey again.
int at508_lock_privatekey(void);
// hash is 32bytes, and signature is 64bytes format in big-endian.
int at508_sign(u8 *hash, u8 *signature, int buflen);
int at508_verify(u8 *hash, u8 *signature, u8 *pubkey, u8 *is_verified);
// random is 32 bytes.
int at508_get_random(u8 *random, int buflen);

#endif /* SRC_AT508_H_ */
