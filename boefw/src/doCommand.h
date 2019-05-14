/*
 * doCommand.h
 *
 *  Created on: Jun 13, 2018
 *      Author: luxq
 */

#ifndef SRC_DOCOMMAND_H_
#define SRC_DOCOMMAND_H_
#include "axu_connector.h"

typedef enum PRET{
	PRET_OK = 0,
	PRET_ERROR = 1,
	PRET_NORES = 2, // don't response.
}PRET;

typedef PRET (*Func)(A_Package *req, A_Package *send);

typedef struct Processor{
	ACmd acmd;
	Func pre_check;
	Func do_func;
}Processor;
extern unsigned char g_random[32];

Processor* processor_get(ACmd acmd);
int make_package_progress(ACmd cmd, u8 progress, char *msg, A_Package *p);
int make_response_ack(A_Package *req, ACmd cmd, u8 ack, A_Package *p);
int make_response_error(A_Package *req, ACmd cmd, u8 err_code, char*err_info, int len, A_Package *p);
void send_upgrade_progress(u8 progress, char *msg);
void hash_random_get();

#endif /* SRC_DOCOMMAND_H_ */
