/**
 * \file
 * \brief  Hardware Interface Functions - I2C bit-bang for SAMD21
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

//#include <asf.h>
#include <hal/i2c_bitbang_boe.h>
#include <stdint.h>
#define DEFAULT_I2C_BUS 1

XGpioPs i2cGpio;
XScuGic i2cGpioIntc;


I2CBuses i2c_buses_default = {
    .pin_scl = {BOE_I2C_SCL},
	.pin_sda = {BOE_I2C_DAT}
};

uint8_t pin_sda, pin_scl;
uint8_t b_init = false;

static int SetupInterruptSystem(XScuGic *GicInstancePtr, XGpioPs *Gpio,
				u16 GpioIntrId);
int boe_i2c_init(XScuGic *Intc, XGpioPs *Gpio, u16 DeviceId, u16 GpioIntrId)
{
	XGpioPs_Config *ConfigPtr;
	int Status;

	/* Initialize the Gpio driver. */
	ConfigPtr = XGpioPs_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		return XST_FAILURE;
	}
	XGpioPs_CfgInitialize(Gpio, ConfigPtr, ConfigPtr->BaseAddr);

	/* Run a self-test on the GPIO device. */
	Status = XGpioPs_SelfTest(Gpio);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* Set the direction for the specified pin to be input */
	XGpioPs_WritePin(Gpio, BOE_I2C_SCL, 0x0);
	XGpioPs_WritePin(Gpio, BOE_I2C_DAT, 0x0);
	I2C_ENABLE();
	/*
	 * Setup the interrupts such that interrupt processing can occur. If
	 * an error occurs then exit.
	 */
	//Status = SetupInterruptSystem(Intc, Gpio, GPIO_INTERRUPT_ID);
	//if (Status != XST_SUCCESS) {
	//	return XST_FAILURE;
	//}

	return XST_SUCCESS;
}


#define boe_i2c_safe() \
	do{\
		if(b_init == false)\
		{\
			boe_i2c_init(&i2cGpioIntc,&i2cGpio,GPIO_DEVICE_ID,GPIO_INTERRUPT_ID);\
			b_init = true;\
		}\
	}while(0);

static int SetupInterruptSystem(XScuGic *GicInstancePtr, XGpioPs *Gpio,
				u16 GpioIntrId)
{
	int Status;

	XScuGic_Config *IntcConfig; /* Instance of the interrupt controller */

	Xil_ExceptionInit();

	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(GicInstancePtr, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


	/*
	 * Connect the interrupt controller interrupt handler to the hardware
	 * interrupt handling logic in the processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
				(Xil_ExceptionHandler)XScuGic_InterruptHandler,
				GicInstancePtr);

	/*
	 * Connect the device driver handler that will be called when an
	 * interrupt for the device occurs, the handler defined above performs
	 * the specific interrupt processing for the device.
	 */
	Status = XScuGic_Connect(GicInstancePtr, GpioIntrId,
				(Xil_ExceptionHandler)XGpioPs_IntrHandler,
				(void *)Gpio);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	/* Enable falling edge interrupts for all the pins in bank 0. */
	XGpioPs_SetIntrType(Gpio, GPIO_BANK, 0x00, 0xFFFFFFFF, 0x00);

	/* Enable the interrupt for the GPIO device. */
	XScuGic_Enable(GicInstancePtr, GpioIntrId);

	/* Enable interrupts in the Processor. */
	Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);

	return XST_SUCCESS;
}

void i2c_discover_buses(int i2c_bitbang_buses[], int max_buses)
{
	boe_i2c_safe();
    i2c_bitbang_buses[0] = DEFAULT_I2C_BUS;

}

void i2c_set_pin(uint8_t sda, uint8_t scl)
{
    pin_sda = sda;
    pin_scl = scl;
    boe_i2c_safe();
}

void i2c_enable(void)
{
	boe_i2c_safe();
    I2C_ENABLE();
    I2C_DATA_HIGH();
    I2C_CLOCK_HIGH();
}

void i2c_disable(void)
{
	boe_i2c_safe();
    I2C_DISABLE();
}


void i2c_send_start(void)
{
	boe_i2c_safe();
    //! Set clock high in case we re-start.
    I2C_CLOCK_HIGH();
    I2C_SET_OUTPUT_HIGH();
    I2C_DATA_LOW();
    I2C_HOLD_DELAY();
    I2C_CLOCK_LOW();
    I2C_HOLD_DELAY();
}

void i2c_send_ack(uint8_t ack)
{
	boe_i2c_safe();
	I2C_CLOCK_DELAY_SEND_ACK();
    if (ack)
    {
        I2C_SET_OUTPUT_LOW();   //!< Low data line indicates an ACK.
        while (I2C_DATA_IN())
        {
            ;
        }
    }
    else
    {
        I2C_SET_OUTPUT_HIGH();  //!< High data line indicates a NACK.
        while (!I2C_DATA_IN())
        {
            ;
        }
    }
    I2C_CLOCK_DELAY_SEND_ACK();
    //! Clock out acknowledgment.
    I2C_CLOCK_HIGH();
    I2C_CLOCK_DELAY_SEND_ACK();
    I2C_CLOCK_LOW();
    I2C_CLOCK_DELAY_SEND_ACK();
}

void i2c_send_stop(void)
{
	boe_i2c_safe();
    I2C_SET_OUTPUT_LOW();
    I2C_CLOCK_DELAY_WRITE_LOW();
    I2C_CLOCK_HIGH();
    I2C_HOLD_DELAY();
    I2C_DATA_HIGH();
    I2C_HOLD_DELAY();
}


void i2c_send_wake_token(void)
{
	boe_i2c_safe();
    I2C_DATA_LOW();
    usleep(80);
    I2C_DATA_HIGH();
}

ATCA_STATUS i2c_send_byte(uint8_t i2c_byte)
{
    ATCA_STATUS status = ATCA_TX_TIMEOUT;

    uint16_t i;

    DISABLE_INTERRUPT();

    //! This avoids spikes but adds an if condition.
    //! We could parametrize the call to I2C_SET_OUTPUT
    //! and translate the msb to OUTSET or OUTCLR,
    //! but then the code would become target specific.
    if (i2c_byte & 0x80)
    {
        I2C_SET_OUTPUT_HIGH();
    }
    else
    {
        I2C_SET_OUTPUT_LOW();
    }

    //! Send 8 bits of data.
    for (i = 0; i < 8; i++)
    {
        I2C_CLOCK_LOW();
        I2C_CLOCK_DELAY_WRITE_LOW();
        if (i2c_byte & 0x80)
        {
            I2C_DATA_HIGH();
        }
        else
        {
            I2C_DATA_LOW();
        }
        I2C_CLOCK_DELAY_WRITE_LOW();

        //! Clock out the data bit.
        I2C_CLOCK_HIGH();

        //! Shifting while clock is high compensates for the time it
        //! takes to evaluate the bit while clock is low.
        //! That way, the low and high time of the clock pin is
        //! almost equal.
        i2c_byte <<= 1;
        I2C_CLOCK_DELAY_WRITE_HIGH();
    }
    //! Clock in last data bit.
    I2C_CLOCK_LOW();
    //I2C_CLOCK_DELAY_READ_LOW();
    //! Set data line to be an input.
    I2C_SET_INPUT();

    I2C_CLOCK_DELAY_READ_LOW();
    //! Wait for the ack.
    I2C_CLOCK_HIGH();
    for (i = 0; i < I2C_ACK_TIMEOUT; i++)
    {
        if (!I2C_DATA_IN())
        {
            status = ATCA_SUCCESS;
            I2C_CLOCK_DELAY_READ_HIGH();
            break;
        }
    }
    I2C_CLOCK_LOW();
    I2C_CLOCK_DELAY_READ_HIGH();

    ENABLE_INTERRUPT();

    return status;
}

ATCA_STATUS i2c_send_bytes(uint8_t count, uint8_t *data)
{
    ATCA_STATUS status = ATCA_TX_TIMEOUT;

    uint8_t i;

    for (i = 0; i < count; i++)
    {
        status = i2c_send_byte(data[i]);
        if (status != ATCA_SUCCESS)
        {
            if (i > 0)
            {
                status = ATCA_TX_FAIL;
            }
            break;
        }
    }

    return status;
}

uint8_t i2c_receive_one_byte(uint8_t ack)
{
    uint8_t i2c_byte = 0;
    uint8_t i;

    DISABLE_INTERRUPT();

    I2C_SET_INPUT();
    I2C_CLOCK_DELAY_READ_HIGH();
    for (i = 0x80, i2c_byte = 0; i; i >>= 1)
    {
        I2C_CLOCK_HIGH();
        I2C_CLOCK_DELAY_READ_HIGH();
        if (I2C_DATA_IN())
        {
            i2c_byte |= i;
        }
        I2C_CLOCK_LOW();
        if (i > 1)
        {
            //! We don't need to delay after the last bit because
            //! it takes time to switch the pin to output for acknowledging.
            I2C_CLOCK_DELAY_READ_LOW();
        }
    }
    i2c_send_ack(ack);

    ENABLE_INTERRUPT();

    return i2c_byte;
}

void i2c_receive_byte(uint8_t *data)
{
    *data = i2c_receive_one_byte(1);
}

void i2c_receive_bytes(uint8_t count, uint8_t *data)
{
    while (--count)
    {
        *data++ = i2c_receive_one_byte(1);
    }
    *data = i2c_receive_one_byte(0);

    i2c_send_stop();
}
