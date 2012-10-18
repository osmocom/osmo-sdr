/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2009, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */
/**
 *  \file
 *
 *  This file contains all the specific code for the
 *  usb_fast_source example.
 */

/*----------------------------------------------------------------------------
 *         Headers
 *----------------------------------------------------------------------------*/

#include <board.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdbool.h>

#include <irq/irq.h>
#include <pio/pio.h>
#include <pio/pio_it.h>
#include <utility/trace.h>
#include <utility/led.h>

#include <memories/flash/flashd.h>

#include <peripherals/rstc/rstc.h>

#include <usb/device/core/USBD.h>
#include <usb/device/core/USBDDriver.h>
#include <usb/device/dfu/dfu.h>

#include "dfu_desc.h"

#include "opcode.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/*----------------------------------------------------------------------------
 *         Definitions
 *----------------------------------------------------------------------------*/

/// Use for power management
#define STATE_IDLE    0
/// The USB device is in suspend state
#define STATE_SUSPEND 4
/// The USB device is in resume state
#define STATE_RESUME  5

//------------------------------------------------------------------------------
//         Internal variables
//------------------------------------------------------------------------------
/// State of USB, for suspend and resume
unsigned char USBState = STATE_IDLE;

/*----------------------------------------------------------------------------
 *         VBus monitoring (optional)
 *----------------------------------------------------------------------------*/

/**  VBus pin instance. */
static const Pin pinVbus = PIN_USB_VBUS;

/* this needs to be in sync with usb-strings.txt !! */
enum dfu_altif {
	ALTIF_APP,
	ALTIF_BOOT,
	ALTIF_RAM,
	ALTIF_FPGA
};

/**
 *  Handles interrupts coming from PIO controllers.
 */
static void ISR_Vbus(const Pin *pPin)
{
    /* Check current level on VBus */
    if (PIO_Get(&pinVbus))
    {
        //TRACE_INFO("VBUS conn\n\r");
        USBD_Connect();
    }
    else
    {
        //TRACE_INFO("VBUS discon\n\r");
        USBD_Disconnect();
    }
}

/**
 *  Configures the VBus pin to trigger an interrupt when the level on that pin
 *  changes.
 */
static void VBus_Configure( void )
{
    /* Configure PIO */
    PIO_Configure(&pinVbus, 1);
    PIO_ConfigureIt(&pinVbus, ISR_Vbus);
    PIO_EnableIt(&pinVbus);

    /* Check current level on VBus */
    if (PIO_Get(&pinVbus))
    {
        /* if VBUS present, force the connect */
        USBD_Connect();
    }
    else
    {
        //TRACE_INFO("discon\n\r");
        USBD_Disconnect();
    }
}

/*----------------------------------------------------------------------------
 *         USB Power Control
 *----------------------------------------------------------------------------*/

#ifdef PIN_USB_POWER_ENA
/** Power Enable A (MicroAB Socket) pin instance. */
static const Pin pinPOnA = PIN_USB_POWER_ENA;
#endif
#ifdef PIN_USB_POWER_ENB
/** Power Enable B (A Socket) pin instance. */
static const Pin pinPOnB = PIN_USB_POWER_ENB;
#endif
#ifdef PIN_USB_POWER_ENC
/** Power Enable C (A Socket) pin instance. */
static const Pin pinPOnC = PIN_USB_POWER_ENC;
#endif
/**
 * Configures the Power Enable pin to disable self power.
 */
static void USBPower_Configure( void )
{
  #ifdef PIN_USB_POWER_ENA
    PIO_Configure(&pinPOnA, 1);
  #endif
  #ifdef PIN_USB_POWER_ENB
    PIO_Configure(&pinPOnB, 1);
  #endif
  #ifdef PIN_USB_POWER_ENC
    PIO_Configure(&pinPOnC, 1);
  #endif
}

/*----------------------------------------------------------------------------
 *         Callbacks re-implementation
 *----------------------------------------------------------------------------*/

//------------------------------------------------------------------------------
/// Invoked when the USB device leaves the Suspended state. By default,
/// configures the LEDs.
//------------------------------------------------------------------------------
void USBDCallbacks_Resumed(void)
{
    // Initialize LEDs
    LED_Configure(USBD_LEDPOWER);
    LED_Set(USBD_LEDPOWER);
    LED_Configure(USBD_LEDUSB);
    LED_Clear(USBD_LEDUSB);
    USBState = STATE_RESUME;
}

//------------------------------------------------------------------------------
/// Invoked when the USB device gets suspended. By default, turns off all LEDs.
//------------------------------------------------------------------------------
void USBDCallbacks_Suspended(void)
{
    // Turn off LEDs
    LED_Clear(USBD_LEDPOWER);
    LED_Clear(USBD_LEDUSB);
    USBState = STATE_SUSPEND;
}

/* USBD callback */
void USBDCallbacks_RequestReceived(const USBGenericRequest *request)
{
	USBDFU_DFU_RequestHandler(request);
}

/* USBD callback */
void USBDDriverCallbacks_InterfaceSettingChanged(unsigned char interface,
						 unsigned char setting)
{
	//TRACE_INFO("DFU: IfSettingChgd(if=%u, alt=%u)\n\r", interface, setting);
}

#define BOOT_FLASH_SIZE		(16 * 1024)

struct flash_part {
	void *base_addr;
	uint32_t size;
};

static const struct flash_part flash_parts[] = {
	[ALTIF_BOOT] = {
		.base_addr =	AT91C_IFLASH0,
		.size =		BOOT_FLASH_SIZE,
	},
	[ALTIF_APP] = {
		.base_addr =	(AT91C_IFLASH + BOOT_FLASH_SIZE),
		.size =		(AT91C_IFLASH0_SIZE - BOOT_FLASH_SIZE),
	},
	[ALTIF_RAM] = {
		.base_addr =	AT91C_IRAM,
		.size =		AT91C_IRAM_SIZE,
	},
	[ALTIF_FPGA] = {
		.base_addr =	0,
		.size =		16*1024*1024, // really no limit
	},
};

/* DFU callback */
int USBDFU_handle_upload(uint8_t altif, unsigned int offset,
			 uint8_t *buf, unsigned int req_len)
{
	return -EINVAL;
#if 0
	struct flash_part *part;
	void *end, *addr;
	uint32_t real_len;

	//TRACE_INFO("DFU: handle_upload(%u, %u, %u)\n\r", altif, offset, req_len);

	if (altif > ARRAY_SIZE(flash_parts))
		return -EINVAL;

	part = &flash_parts[altif];

	addr = part->base_addr + offset;
	end = part->base_addr + part->size;

	real_len = end - addr;
	if (req_len < real_len)
		real_len = req_len;

	LED_Set(USBD_LEDOTHER);
	memcpy(buf, addr, real_len);
	LED_Clear(USBD_LEDOTHER);

	return real_len;
#endif
}

extern unsigned short g_usDataType;

static uint8_t fpgaBuf[1024];
static uint fpgaBufStart; // file offset of first byte in buffer
static uint fpgaBufEnd;
static uint fpgaBufFill;
static uint fpgaBufRPtr;
static uint fpgaPushedAddr;

short int ispProcessVME();
void EnableHardware();

void fpgaPushAddr()
{
	fpgaPushedAddr = fpgaBufRPtr;
}

void fpgaPopAddr()
{
	printf("*");
	fpgaBufRPtr = fpgaPushedAddr;
}

uint8_t fpgaGetByte()
{
	uint8_t res = fpgaBuf[fpgaBufRPtr - fpgaBufStart];
	fpgaBufRPtr++;

	return res;
}

static int fpgaFlash(unsigned int offset, const uint8_t* buf, unsigned int len)
{
	int i;

	if(offset == 0) {
		for(i = 0; i < len; i++)
			fpgaBuf[i] = buf[i];
		fpgaBufStart = 0;
		fpgaBufEnd = len;
		fpgaBufFill = len;
		fpgaBufRPtr = 0;

		*((uint32_t*)(0x400e0410)) = (1 << 11);
		*((uint32_t*)(0x400e0e00 + 0x44)) = (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8);
		*((uint32_t*)(0x400e0e00 + 0x60)) = (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8);
		*((uint32_t*)(0x400e0e00 + 0x54)) = (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8);
		*((uint32_t*)(0x400e0e00 + 0x10)) = (1 << 5) | (1 << 7) | (1 << 8);
		*((uint32_t*)(0x400e0e00 + 0x00)) = (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8);

		if(fpgaGetByte())
			g_usDataType = COMPRESS;
		else g_usDataType = 0;

	    EnableHardware();
	} else {
		for(i = 0; i < len; i++)
			fpgaBuf[fpgaBufFill + i] = buf[i];
		fpgaBufEnd += len;
		fpgaBufFill += len;
	}
/*
	printf("\n\rs:%d,e:%d,f:%d,r:%d,p:%d\n",
		fpgaBufStart, fpgaBufEnd, fpgaBufFill, fpgaBufRPtr, fpgaPushedAddr);
*/
	while(fpgaBufEnd - fpgaBufRPtr > 192) {
		if((i = ispProcessVME()) < 0)
			return -1;
	}

	if(fpgaBufFill > 384) {
		uint moveby = fpgaBufFill - 384;
		uint movelen = fpgaBufFill - movelen;

		for(i = 0; i < movelen; i++)
			fpgaBuf[i] = fpgaBuf[i + moveby];
		fpgaBufStart += moveby;
		fpgaBufFill -= moveby;
/*
		printf("\n\rs:%d,e:%d,f:%d,r:%d,p:%d\n",
			fpgaBufStart, fpgaBufEnd, fpgaBufFill, fpgaBufRPtr, fpgaPushedAddr);
*/
	}

	return 0;
}

/* DFU callback */
int USBDFU_handle_dnload(uint8_t altif, unsigned int offset,
			 uint8_t *buf, unsigned int len)
{
	struct flash_part *part;
	void *end, *addr;
	int rc;

	TRACE_INFO("DFU: handle_dnload(%u, %u, %u)\n\r", altif, offset, len);

	if (altif > ARRAY_SIZE(flash_parts))
		return -EINVAL;

	part = &flash_parts[altif];

	addr = part->base_addr + offset;
	end = part->base_addr + part->size;

	if (addr + len > end) {
		TRACE_ERROR("Write beyond end (%u)\n\r", altif);
		g_dfu.status = DFU_STATUS_errADDRESS;
		return DFU_RET_STALL;
	}

	switch (altif) {
	case ALTIF_APP:
		/* SAM3U Errata 46.2.1.3 */
		SetFlashWaitState(6);
		LED_Set(USBD_LEDOTHER);
		rc = FLASHD_Write(addr, buf, len);
		LED_Clear(USBD_LEDOTHER);
		/* SAM3U Errata 46.2.1.3 */
		SetFlashWaitState(2);
		if (rc != 0) {
			TRACE_ERROR("Write error (%u)\n\r", altif);
			g_dfu.status = DFU_STATUS_errPROG;
			return DFU_RET_STALL;
		}
		break;

	case ALTIF_FPGA:
		LED_Set(USBD_LEDOTHER);
		rc = fpgaFlash(offset, buf, len);
		LED_Clear(USBD_LEDOTHER);
		/* SAM3U Errata 46.2.1.3 */
		if (rc != 0) {
			TRACE_ERROR("FPGA error (ofs %d)\n\r", fpgaBufRPtr);
			g_dfu.status = DFU_STATUS_errPROG;
			return DFU_RET_STALL;
		}
		break;

	default:
		TRACE_WARNING("Not implemented (%u)\n\r", altif);
		g_dfu.status = DFU_STATUS_errTARGET;
		break;
	}

	return DFU_RET_ZLP;
}

/* DFU callback */
void dfu_drv_updstatus(void)
{
	TRACE_INFO("DFU: updstatus()\n\r");

	if (g_dfu.state == DFU_STATE_dfuMANIFEST_SYNC)
		g_dfu.state = DFU_STATE_dfuMANIFEST;
}

/*----------------------------------------------------------------------------
 *         Exported functions
 *----------------------------------------------------------------------------*/

extern void USBD_IrqHandler(void);
/*
static const char *rst_type_strs[8] = {
	"General", "Backup", "Watchdog", "Softare", "User", "5", "6", "7"
};
*/
int main(void)
{
    volatile uint8_t usbConn = 0;
    unsigned long rst_type;

    TRACE_CONFIGURE(DBGU_STANDARD, 115200, BOARD_MCK);

    printf("-- Osmocom USB DFU (" BOARD_NAME ") " GIT_REVISION " --\n\r");
    printf("-- Compiled: %s %s --\n\r", __DATE__, __TIME__);

    rst_type = (RSTC_GetStatus() >> 8) & 0x7;
    //printf("-- Reset type: %s --\n\r", rst_type_strs[rst_type]);

    chipid_to_usbserial();

    /* If they are present, configure Vbus & Wake-up pins */
    PIO_InitializeInterrupts(0);

    /* Initialize all USB power (off) */
    USBPower_Configure();

    /* Audio STREAM LED */
    LED_Configure(USBD_LEDOTHER);

    USBDFU_Initialize(&dfu_descriptors);

    /* connect if needed */
    VBus_Configure();

    static int state = 0;

    /* Infinite loop */
    while (1)
    {
        if (USBD_GetState() < USBD_STATE_CONFIGURED)
        {
            usbConn = 0;
            continue;
        } else
		usbConn = 1;
	/* This works, but breaks JTAG debugging */
	//__WFE();
    }
}
/** \endcond */
