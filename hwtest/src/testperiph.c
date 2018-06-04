/*
 *
 * Xilinx, Inc.
 * XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A 
 * COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR 
 * STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION 
 * IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE 
 * FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION
 * XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO 
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO 
 * ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE 
 * FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY 
 * AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * 
 *
 * This file is a generated sample test application.
 *
 * This application is intended to test and/or illustrate some 
 * functionality of your system.  The contents of this file may
 * vary depending on the IP in your system and may use existing
 * IP driver functions.  These drivers will be generated in your
 * SDK application project when you run the "Generate Libraries" menu item.
 *
 */

#include <stdio.h>
#include "xparameters.h"
#include "xil_cache.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "scugic_header.h"
#include "xiicps.h"
#include "iicps_header.h"
#include "xttcps.h"
#include "ttcps_header.h"
#include "xuartps.h"
#include "uartps_header.h"
#include "uartps_intr_header.h"
#include "xwdtps.h"
#include "wdtps_header.h"
int main () 
{
   static XScuGic intc;
   static XTtcPs psu_ttc_0;
   static XTtcPs psu_ttc_1;
   static XTtcPs psu_ttc_2;
   static XTtcPs psu_ttc_3;
   static XUartPs psu_uart_1;
   Xil_ICacheEnable();
   Xil_DCacheEnable();
   print("---Entering main---\n\r");


	   {
	      int Status;

	      print("\r\n Running ScuGicSelfTestExample() for psu_acpu_gic...\r\n");

	      Status = ScuGicSelfTestExample(XPAR_PSU_ACPU_GIC_DEVICE_ID);

	      if (Status == 0) {
	         print("ScuGicSelfTestExample PASSED\r\n");
	      }
	      else {
	         print("ScuGicSelfTestExample FAILED\r\n");
	      }
	   }

	   {
	       int Status;

	       Status = ScuGicInterruptSetup(&intc, XPAR_PSU_ACPU_GIC_DEVICE_ID);
	       if (Status == 0) {
	          print("ScuGic Interrupt Setup PASSED\r\n");
	       }
	       else {
	         print("ScuGic Interrupt Setup FAILED\r\n");
	      }
	   }



   {
      int Status;

      print("\r\n Running IicPsSelfTestExample() for psu_i2c_0...\r\n");

      Status = IicPsSelfTestExample(XPAR_PSU_I2C_0_DEVICE_ID);

      if (Status == 0) {
         print("IicPsSelfTestExample PASSED\r\n");
      }
      else {
         print("IicPsSelfTestExample FAILED\r\n");
      }
   }



   {
      int Status;

      print("\r\n Running IicPsSelfTestExample() for psu_i2c_1...\r\n");

      Status = IicPsSelfTestExample(XPAR_PSU_I2C_1_DEVICE_ID);

      if (Status == 0) {
         print("IicPsSelfTestExample PASSED\r\n");
      }
      else {
         print("IicPsSelfTestExample FAILED\r\n");
      }
   }


   {
      int Status;

      print("\r\n Running Interrupt Test  for psu_ttc_0...\r\n");

      Status = TmrInterruptExample(&psu_ttc_0, \
				XPAR_PSU_TTC_0_DEVICE_ID, \
				XPAR_PSU_TTC_0_INTR, &intc);

      if (Status == 0) {
         print("TtcIntrExample PASSED\r\n");
      }
      else {
         print("TtcIntrExample FAILED\r\n");
      }

   }


   {
      int Status;

      print("\r\n Running Interrupt Test  for psu_ttc_1...\r\n");

      Status = TmrInterruptExample(&psu_ttc_1, \
				XPAR_PSU_TTC_3_DEVICE_ID, \
				XPAR_PSU_TTC_3_INTR, &intc);

      if (Status == 0) {
         print("TtcIntrExample PASSED\r\n");
      }
      else {
         print("TtcIntrExample FAILED\r\n");
      }

   }


   {
      int Status;

      print("\r\n Running Interrupt Test  for psu_ttc_2...\r\n");

      Status = TmrInterruptExample(&psu_ttc_2, \
				XPAR_PSU_TTC_6_DEVICE_ID, \
				XPAR_PSU_TTC_6_INTR, &intc);

      if (Status == 0) {
         print("TtcIntrExample PASSED\r\n");
      }
      else {
         print("TtcIntrExample FAILED\r\n");
      }

   }


   {
      int Status;

      print("\r\n Running Interrupt Test  for psu_ttc_3...\r\n");

      Status = TmrInterruptExample(&psu_ttc_3, \
				XPAR_PSU_TTC_9_DEVICE_ID, \
				XPAR_PSU_TTC_9_INTR, &intc);

      if (Status == 0) {
         print("TtcIntrExample PASSED\r\n");
      }
      else {
         print("TtcIntrExample FAILED\r\n");
      }

   }


   /*
    * Peripheral Test will not be run for psu_uart_0
    * because it has been selected as the STDOUT device
    */




   {
      int Status;

      print("\r\nRunning UartPsPolledExample() for psu_uart_1...\r\n");
      Status = UartPsPolledExample(XPAR_PSU_UART_1_DEVICE_ID);
      if (Status == 0) {
         print("UartPsPolledExample PASSED\r\n");
      }
      else {
         print("UartPsPolledExample FAILED\r\n");
      }
   }
   {
      int Status;

      print("\r\n Running Interrupt Test for psu_uart_1...\r\n");

      Status = UartPsIntrExample(&intc, &psu_uart_1, \
                                  XPAR_PSU_UART_1_DEVICE_ID, \
                                  XPAR_PSU_UART_1_INTR);

      if (Status == 0) {
         print("UartPsIntrExample PASSED\r\n");
      }
      else {
         print("UartPsIntrExample FAILED\r\n");
      }

   }



   {
      int Status;

      print("\r\n Running WdtPsSelfTestExample() for psu_wdt_0...\r\n");

      Status = WdtPsSelfTestExample(XPAR_PSU_WDT_0_DEVICE_ID);

      if (Status == 0) {
         print("WdtPsSelfTestExample PASSED\r\n");
      }
      else {
         print("WdtPsSelfTestExample FAILED\r\n");
      }
   }



   {
      int Status;

      print("\r\n Running WdtPsSelfTestExample() for psu_wdt_1...\r\n");

      Status = WdtPsSelfTestExample(XPAR_PSU_WDT_1_DEVICE_ID);

      if (Status == 0) {
         print("WdtPsSelfTestExample PASSED\r\n");
      }
      else {
         print("WdtPsSelfTestExample FAILED\r\n");
      }
   }


   print("---Exiting main---\n\r");
   Xil_DCacheDisable();
   Xil_ICacheDisable();
   return 0;
}
