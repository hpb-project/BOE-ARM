/**
 * \file
 * \brief  definitions for bit-banged I2C
 *
 * \copyright (c) 2017 Microchip Technology Inc. and its subsidiaries.
 *            You may use this software and any derivatives exclusively with
 *            Microchip products.
 *
 * \page License
 *
 * (c) 2017 Microchip Technology Inc. and its subsidiaries. You may use this
 * software and any derivatives exclusively with Microchip products.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
 * EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
 * WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
 * PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
 * WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.
 *
 * IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
 * INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
 * WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
 * BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
 * FULLEST EXTENT ALLOWED BY LAW, MICROCHIPS TOTAL LIABILITY ON ALL CLAIMS IN
 * ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
 * TERMS.
 */

#ifndef I2C_BITBANG_SAMD21_H_
#define I2C_BITBANG_SAMD21_H_

#include "atca_status.h"
#include "xparameters.h"
#include "xgpiops.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xstatus.h"
#include "xplatform_info.h"
#include <sleep.h>


#define MAX_I2C_BUSES   1       //The MAX_I2C_BUSES is the number of free pins in samd21 xplained pro
#define BOE_I2C_DAT         (29)
#define BOE_I2C_SCL			(28)
#define BOE_INPUT       (0)
#define BOE_OUTPUT      (1)

extern XGpioPs i2cGpio;
extern XScuGic i2cGpioIntc;
#define GPIO_DEVICE_ID		XPAR_XGPIOPS_0_DEVICE_ID
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#define GPIO_INTERRUPT_ID	XPAR_XGPIOPS_0_INTR
#define GPIO_BANK			XGPIOPS_BANK0  /* Bank 0 of the GPIO Device */

typedef struct
{
    uint8_t pin_sda[MAX_I2C_BUSES];
    uint8_t pin_scl[MAX_I2C_BUSES];
} I2CBuses;

extern I2CBuses i2c_buses_default;

extern uint8_t pin_sda;
extern uint8_t pin_scl;



#   define I2C_ENABLE()         {  XGpioPs_SetDirectionPin(&i2cGpio, BOE_I2C_DAT, BOE_OUTPUT);\
									XGpioPs_SetOutputEnablePin(&i2cGpio, BOE_I2C_DAT, 1);\
									XGpioPs_SetDirectionPin(&i2cGpio, BOE_I2C_SCL, BOE_OUTPUT);\
									XGpioPs_SetOutputEnablePin(&i2cGpio, BOE_I2C_SCL, 1);\
								}

#   define I2C_DISABLE()        {  XGpioPs_SetDirectionPin(&i2cGpio, BOE_I2C_DAT, BOE_INPUT);\
									XGpioPs_SetDirectionPin(&i2cGpio, BOE_I2C_SCL, BOE_INPUT);}
#   define I2C_CLOCK_LOW()       {XGpioPs_WritePin(&i2cGpio, BOE_I2C_SCL, 0x0);}
#   define I2C_CLOCK_HIGH()      {XGpioPs_WritePin(&i2cGpio, BOE_I2C_SCL, 0x1);}
#   define I2C_DATA_LOW()        {XGpioPs_WritePin(&i2cGpio, BOE_I2C_DAT, 0x0);}
#   define I2C_DATA_HIGH()       {XGpioPs_WritePin(&i2cGpio, BOE_I2C_DAT, 0x1);}
#   define I2C_DATA_IN()         XGpioPs_ReadPin(&i2cGpio, BOE_I2C_DAT)

#   define I2C_SET_OUTPUT()      {  XGpioPs_SetDirectionPin(&i2cGpio, BOE_I2C_DAT, BOE_OUTPUT);\
									 XGpioPs_SetOutputEnablePin(&i2cGpio, BOE_I2C_DAT, 1);\
								 }
#   define I2C_SET_OUTPUT_HIGH() { I2C_SET_OUTPUT(); I2C_DATA_HIGH(); }
#   define I2C_SET_OUTPUT_LOW()  { I2C_SET_OUTPUT(); I2C_DATA_LOW(); }
#   define I2C_SET_INPUT()       {  XGpioPs_SetDirectionPin(&i2cGpio, BOE_I2C_DAT, BOE_INPUT);\
									 XGpioPs_SetOutputEnablePin(&i2cGpio, BOE_I2C_DAT, 1);\
								}
//#   define DISABLE_INTERRUPT()   {XGpioPs_IntrDisable(&i2cGpio, GPIO_BANK, (1 << BOE_I2C_DAT)|(1<<BOE_I2C_SCL));\
//									XScuGic_Disable(&i2cGpioIntc, GPIO_INTERRUPT_ID);}
#   define DISABLE_INTERRUPT()   {}

//#   define ENABLE_INTERRUPT()    {XGpioPs_IntrEnable(&i2cGpio, GPIO_BANK, (1 << BOE_I2C_DAT)|(1<<BOE_I2C_SCL));\
//									XScuGic_Enable(&i2cGpioIntc, GPIO_INTERRUPT_ID);}
#   define ENABLE_INTERRUPT()    {}


#define I2C_CLOCK_DELAY_WRITE_LOW()  usleep(40)
#define I2C_CLOCK_DELAY_WRITE_HIGH() usleep(40)
#define I2C_CLOCK_DELAY_READ_LOW()   usleep(40)
#define I2C_CLOCK_DELAY_READ_HIGH()  usleep(40)
#define I2C_CLOCK_DELAY_SEND_ACK()   usleep(40)
//! This delay is inserted to make the Start and Stop hold time at least 250 ns.
#define I2C_HOLD_DELAY()    usleep(40)




//! loop count when waiting for an acknowledgment
#define I2C_ACK_TIMEOUT                 (300)


/**
 * \brief Set I2C data and clock pin.
 *        Other functions will use these pins.
 *
 * \param[in] sda  definition of GPIO pin to be used as data pin
 * \param[in] scl  definition of GPIO pin to be used as clock pin
 */
void i2c_set_pin(uint8_t sda, uint8_t scl);


/**
 * \brief  Assigns the logical bus number for discovering the devices
 *
 *
 * \param[in]  i2c_bitbang_buses         The logical bus numbers are assigned to the variables.
 * \param[in]  max_buses                 Maximum number of bus used for discovering.
 */

void i2c_discover_buses(int i2c_bitbang_buses[], int max_buses);

/**
 * \brief Configure GPIO pins for I2C clock and data as output.
 */
void i2c_enable(void);

/**
 * \brief Configure GPIO pins for I2C clock and data as input.
 */
void i2c_disable(void);


/**
 * \brief Send a START condition.
 */
void i2c_send_start(void);

/**
 * \brief Send an ACK or NACK (after receive).
 *
 * \param[in] ack  0: NACK, else: ACK
 */
void i2c_send_ack(uint8_t ack);

/**
 * \brief Send a STOP condition.
 */
void i2c_send_stop(void);

/**
 * \brief Send a Wake Token.
 */
void i2c_send_wake_token(void);

/**
 * \brief Send one byte.
 *
 * \param[in] i2c_byte  byte to write
 *
 * \return ATCA_STATUS
 */
ATCA_STATUS i2c_send_byte(uint8_t i2c_byte);

/**
 * \brief Send a number of bytes.
 *
 * \param[in] count  number of bytes to send
 * \param[in] data   pointer to buffer containing bytes to send
 *
 * \return ATCA_STATUS
 */
ATCA_STATUS i2c_send_bytes(uint8_t count, uint8_t *data);

/**
 * \brief Receive one byte (MSB first).
 *
 * \param[in] ack  0:NACK, else:ACK
 *
 * \return Number of bytes received
 */
uint8_t i2c_receive_one_byte(uint8_t ack);

/**
 * \brief Receive one byte and send ACK.
 *
 * \param[out] data  pointer to received byte
 */
void i2c_receive_byte(uint8_t *data);

/**
 * \brief Receive a number of bytes.
 *
 * \param[out] data   pointer to receive buffer
 * \param[in]  count  number of bytes to receive
 */
void i2c_receive_bytes(uint8_t count, uint8_t *data);

#endif /* I2C_BITBANG_SAMD21_H_ */
