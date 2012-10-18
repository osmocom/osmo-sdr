/* This file uses lots of code from the Atmel reference library:
 * ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2008, Atmel Corporation
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

#include <stddef.h>
#include "../crt/stdio.h"
#include "../board.h"
#include "../driver/usbdhs.h"
#include "../driver/sys.h"
#include "../driver/flash.h"
#include "usbdevice.h"
#include "usb.h"
#include "opcode.h"

typedef struct {
	u8 bLength;
	u8 bDescriptorType;
	u8 bmAttributes;
	u16 wDetachTimeOut;
	u16 wTransferSize;
	u16 bcdDFUVersion;
} __attribute__((packed)) USBDFUFunctionDescriptor;

typedef struct {
	USBConfigurationDescriptor configuration;
	USBInterfaceDescriptor interfaces[3];
	USBDFUFunctionDescriptor function;
}__attribute__((packed)) USBDFUDescriptor;

typedef enum {
	DFU_STATE_appIDLE = 0,
	DFU_STATE_appDETACH = 1,
	DFU_STATE_dfuIDLE = 2,
	DFU_STATE_dfuDNLOAD_SYNC = 3,
	DFU_STATE_dfuDNBUSY = 4,
	DFU_STATE_dfuDNLOAD_IDLE = 5,
	DFU_STATE_dfuMANIFEST_SYNC = 6,
	DFU_STATE_dfuMANIFEST = 7,
	DFU_STATE_dfuMANIFEST_WAIT_RST = 8,
	DFU_STATE_dfuUPLOAD_IDLE = 9,
	DFU_STATE_dfuERROR = 10,
} DFUState;

typedef struct {
	uint8_t bStatus;
	uint8_t bwPollTimeout[3];
	uint8_t bState;
	uint8_t iString;
}__attribute__((packed)) DFUStatus;

typedef struct {
	int status;
	DFUState state;
	Bool pastManifest;
	uint totalBytes;
	u32 buffer[BOARD_DFU_PAGE_SIZE / 4 + 4];
} DFUData;

static DFUData g_dfuData = {
    .state = DFU_STATE_appIDLE,
    .pastManifest = False,
    .totalBytes = 0,
};

#define USB_REQ_DFU_DETACH      0x00
#define USB_REQ_DFU_DNLOAD      0x01
#define USB_REQ_DFU_UPLOAD      0x02
#define USB_REQ_DFU_GETSTATUS   0x03
#define USB_REQ_DFU_CLRSTATUS   0x04
#define USB_REQ_DFU_GETSTATE    0x05
#define USB_REQ_DFU_ABORT       0x06

#define DFU_STATUS_OK                   0x00
#define DFU_STATUS_errTARGET            0x01
#define DFU_STATUS_errFILE              0x02
#define DFU_STATUS_errWRITE             0x03
#define DFU_STATUS_errERASE             0x04
#define DFU_STATUS_errCHECK_ERASED      0x05
#define DFU_STATUS_errPROG              0x06
#define DFU_STATUS_errVERIFY            0x07
#define DFU_STATUS_errADDRESS           0x08
#define DFU_STATUS_errNOTDONE           0x09
#define DFU_STATUS_errFIRMWARE          0x0a
#define DFU_STATUS_errVENDOR            0x0b
#define DFU_STATUS_errUSBR              0x0c
#define DFU_STATUS_errPOR               0x0d
#define DFU_STATUS_errUNKNOWN           0x0e
#define DFU_STATUS_errSTALLEDPKT        0x0f

#define DFU_RET_NOTHING 0
#define DFU_RET_ZLP     1
#define DFU_RET_STALL   2

#define STR_MANUFACTURER 1
#define STR_PRODUCT 2
#define STR_SERIAL 3
#define STR_CONFIG 4
#define STR_DFU_IF_RADIOAPP 5
#define STR_DFU_IF_LOADERAPP 6
#define STR_DFU_IF_FPGAIMAGE 7
#define NUM_STRINGS 7

#define USBStringDescriptor_LENGTH(length) ((length) * 2 + 2)
#define USBStringDescriptor_ENGLISH_US 0x09, 0x04
#define USBStringDescriptor_UNICODE(ascii) (ascii), 0

static const u8 languageIdDescriptor[] = {
	USBStringDescriptor_LENGTH(1),
	USBGenericDescriptor_STRING,
	USBStringDescriptor_ENGLISH_US
};

static const u8 strManufacturer[] = {
	// sysmocom - systems for mobile communications GmbH
	USBStringDescriptor_LENGTH(49),
	USBGenericDescriptor_STRING,
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE('y'),
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('c'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('-'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE('y'),
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE('t'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('f'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('r'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('b'),
	USBStringDescriptor_UNICODE('i'),
	USBStringDescriptor_UNICODE('l'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('c'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('u'),
	USBStringDescriptor_UNICODE('n'),
	USBStringDescriptor_UNICODE('i'),
	USBStringDescriptor_UNICODE('c'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('t'),
	USBStringDescriptor_UNICODE('i'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('n'),
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('G'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('b'),
	USBStringDescriptor_UNICODE('H')
};

static const u8 strProduct[] = {
	// OsmoSDR Software Defined Radio
	USBStringDescriptor_LENGTH(30),
	USBGenericDescriptor_STRING,
	USBStringDescriptor_UNICODE('O'),
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('S'),
	USBStringDescriptor_UNICODE('D'),
	USBStringDescriptor_UNICODE('R'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('S'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('f'),
	USBStringDescriptor_UNICODE('t'),
	USBStringDescriptor_UNICODE('w'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('r'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('D'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE('f'),
	USBStringDescriptor_UNICODE('i'),
	USBStringDescriptor_UNICODE('n'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE('d'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('R'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('d'),
	USBStringDescriptor_UNICODE('i'),
	USBStringDescriptor_UNICODE('o')
};

static u8 strSerial[] = {
	// _10xxx2416030xxx
	USBStringDescriptor_LENGTH(16),
	USBGenericDescriptor_STRING,
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.'),
	USBStringDescriptor_UNICODE('.')
};

static const u8 strConfig[] = {
	// OsmoSDR Device Firmware Upgrade
	USBStringDescriptor_LENGTH(31),
	USBGenericDescriptor_STRING,
	USBStringDescriptor_UNICODE('O'),
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('S'),
	USBStringDescriptor_UNICODE('D'),
	USBStringDescriptor_UNICODE('R'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('D'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE('v'),
	USBStringDescriptor_UNICODE('i'),
	USBStringDescriptor_UNICODE('c'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('F'),
	USBStringDescriptor_UNICODE('i'),
	USBStringDescriptor_UNICODE('r'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('w'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('r'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('U'),
	USBStringDescriptor_UNICODE('p'),
	USBStringDescriptor_UNICODE('g'),
	USBStringDescriptor_UNICODE('r'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('d'),
	USBStringDescriptor_UNICODE('e')
};

static const u8 strRadioApp[] = {
	// OsmoSDR DFU Interface - Radio Application
	USBStringDescriptor_LENGTH(41),
	USBGenericDescriptor_STRING,
	USBStringDescriptor_UNICODE('O'),
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('S'),
	USBStringDescriptor_UNICODE('D'),
	USBStringDescriptor_UNICODE('R'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('D'),
	USBStringDescriptor_UNICODE('F'),
	USBStringDescriptor_UNICODE('U'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('I'),
	USBStringDescriptor_UNICODE('n'),
	USBStringDescriptor_UNICODE('t'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE('r'),
	USBStringDescriptor_UNICODE('f'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('c'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('-'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('R'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('d'),
	USBStringDescriptor_UNICODE('i'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('A'),
	USBStringDescriptor_UNICODE('p'),
	USBStringDescriptor_UNICODE('p'),
	USBStringDescriptor_UNICODE('l'),
	USBStringDescriptor_UNICODE('i'),
	USBStringDescriptor_UNICODE('c'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('t'),
	USBStringDescriptor_UNICODE('i'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('n')
};

static const u8 strLoaderApp[] = {
	// OsmoSDR DFU Interface - Bootloader
	USBStringDescriptor_LENGTH(34),
	USBGenericDescriptor_STRING,
	USBStringDescriptor_UNICODE('O'),
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('S'),
	USBStringDescriptor_UNICODE('D'),
	USBStringDescriptor_UNICODE('R'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('D'),
	USBStringDescriptor_UNICODE('F'),
	USBStringDescriptor_UNICODE('U'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('I'),
	USBStringDescriptor_UNICODE('n'),
	USBStringDescriptor_UNICODE('t'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE('r'),
	USBStringDescriptor_UNICODE('f'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('c'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('-'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('B'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('t'),
	USBStringDescriptor_UNICODE('l'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('d'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE('r')
};

static const u8 strFPGAImage[] = {
	// OsmoSDR DFU Interface - FPGA Image
	USBStringDescriptor_LENGTH(34),
	USBGenericDescriptor_STRING,
	USBStringDescriptor_UNICODE('O'),
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('S'),
	USBStringDescriptor_UNICODE('D'),
	USBStringDescriptor_UNICODE('R'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('D'),
	USBStringDescriptor_UNICODE('F'),
	USBStringDescriptor_UNICODE('U'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('I'),
	USBStringDescriptor_UNICODE('n'),
	USBStringDescriptor_UNICODE('t'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE('r'),
	USBStringDescriptor_UNICODE('f'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('c'),
	USBStringDescriptor_UNICODE('e'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('-'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('F'),
	USBStringDescriptor_UNICODE('P'),
	USBStringDescriptor_UNICODE('G'),
	USBStringDescriptor_UNICODE('A'),
	USBStringDescriptor_UNICODE(' '),
	USBStringDescriptor_UNICODE('I'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('a'),
	USBStringDescriptor_UNICODE('g'),
	USBStringDescriptor_UNICODE('e')
};

static const u8* strings[NUM_STRINGS + 1] = {
	languageIdDescriptor,
	strManufacturer,
	strProduct,
	strSerial,
	strConfig,
	strRadioApp,
	strLoaderApp,
	strFPGAImage,
};

static const USBDeviceDescriptor usbDeviceDescriptor = {
	.bLength = sizeof(USBDeviceDescriptor),
	.bDescriptorType = USBGenericDescriptor_DEVICE,
	.bcdUSB = USBDeviceDescriptor_USB2_00,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = CHIP_USB_ENDPOINTS_MAXPACKETSIZE(0),
	.idVendor = BOARD_USB_VENDOR,
	.idProduct = BOARD_USB_PRODUCT,
	.bcdDevice = BOARD_USB_RELEASE,
	.iManufacturer = STR_MANUFACTURER,
	.iProduct = STR_PRODUCT,
	.iSerialNumber = STR_SERIAL,
	.bNumConfigurations = 1
};

#define USB_DFU_CAN_DOWNLOAD    (1 << 0)
#define USB_DFU_CAN_UPLOAD      (1 << 1)
#define USB_DFU_MANIFEST_TOL    (1 << 2)
#define USB_DFU_WILL_DETACH     (1 << 3)

static const USBDFUDescriptor usbConfigurationDescriptor = {
	.configuration = {
		.bLength = sizeof(USBConfigurationDescriptor),
		.bDescriptorType = USBGenericDescriptor_CONFIGURATION,
		.wTotalLength = sizeof(USBConfigurationDescriptor) + 3 * sizeof(USBInterfaceDescriptor) + sizeof(USBDFUFunctionDescriptor),
		.bNumInterfaces = BOARD_USB_NUMINTERFACES,
		.bConfigurationValue = 1,
		.iConfiguration = STR_CONFIG,
		.bmAttributes = BOARD_USB_BMATTRIBUTES,
		.bMaxPower = 100
	},
	.interfaces = {
		{
			.bLength = sizeof(USBInterfaceDescriptor),
			.bDescriptorType = USBGenericDescriptor_INTERFACE,
			.bInterfaceNumber = 0,
			.bAlternateSetting = 0,
			.bNumEndpoints = 0,
			.bInterfaceClass = 0xfe,
			.bInterfaceSubClass = 1,
			.iInterface = STR_DFU_IF_RADIOAPP,
			.bInterfaceProtocol = 2
		},
		{
			.bLength = sizeof(USBInterfaceDescriptor),
			.bDescriptorType = USBGenericDescriptor_INTERFACE,
			.bInterfaceNumber = 0,
			.bAlternateSetting = 1,
			.bNumEndpoints = 0,
			.bInterfaceClass = 0xfe,
			.bInterfaceSubClass = 1,
			.iInterface = STR_DFU_IF_LOADERAPP,
			.bInterfaceProtocol = 2
		},
		{
			.bLength = sizeof(USBInterfaceDescriptor),
			.bDescriptorType = USBGenericDescriptor_INTERFACE,
			.bInterfaceNumber = 0,
			.bAlternateSetting = 2,
			.bNumEndpoints = 0,
			.bInterfaceClass = 0xfe,
			.bInterfaceSubClass = 1,
			.iInterface = STR_DFU_IF_FPGAIMAGE,
			.bInterfaceProtocol = 2
		}
	},
	.function = {
        .bLength = sizeof(USBDFUFunctionDescriptor),
        .bDescriptorType = USBFunctionDescriptor_DFU,
        .bmAttributes = USB_DFU_CAN_DOWNLOAD,
        .wDetachTimeOut = 0xff00,
        .wTransferSize = BOARD_DFU_PAGE_SIZE,
        .bcdDFUVersion = 0x0100,
	}
};

static const USBDeviceQualifierDescriptor usbDeviceQualifierDescriptor = {
	.bLength = sizeof(USBDeviceQualifierDescriptor),
	.bDescriptorType = USBGenericDescriptor_DEVICEQUALIFIER,
	.bcdUSB = USBDeviceDescriptor_USB2_00,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = CHIP_USB_ENDPOINTS_MAXPACKETSIZE(0),
	.bNumConfigurations = 1,
	.bReserved = 0,
};

static u8 interfaces[BOARD_USB_NUMINTERFACES];

USBDevice usbDevice = {
	.fsDeviceDescriptor = &usbDeviceDescriptor,
	.fsConfigurationDescriptor = (const USBConfigurationDescriptor*)&usbConfigurationDescriptor,
	.fsQualifierDescriptor = &usbDeviceQualifierDescriptor,
	.fsOtherSpeedDescriptor = (const USBConfigurationDescriptor*)&usbConfigurationDescriptor,

	.hsDeviceDescriptor = &usbDeviceDescriptor,
	.hsConfigurationDescriptor = (const USBConfigurationDescriptor*)&usbConfigurationDescriptor,
	.hsQualifierDescriptor = &usbDeviceQualifierDescriptor,
	.hsOtherSpeedDescriptor = (const USBConfigurationDescriptor*)&usbConfigurationDescriptor,

	.strings = strings,
	.numStrings = NUM_STRINGS,

	.activeConfiguration = 0,
	.remoteWakeupEnabled = False,
	.interfaces = interfaces
};

static void updateStatus(void)
{
	if(g_dfuData.state == DFU_STATE_dfuMANIFEST_SYNC)
		g_dfuData.state = DFU_STATE_dfuMANIFEST;
}

static void sendStatus(void)
{
	/* has to be static as USBD_Write is async ? */
	static DFUStatus dstat;

	updateStatus();

	/* send status response */
	dstat.bStatus = g_dfuData.status;
	dstat.bState = g_dfuData.state;
	dstat.iString = 0;
	/* FIXME: set dstat.bwPollTimeout */

	//TRACE_DEBUG("handle_getstatus(%u, %u)\n\r", dstat.bStatus, dstat.bState);
	usbdhs_write(0, &dstat, sizeof(DFUStatus), NULL, NULL);
}

static void sendState(void)
{
	static u8 state;
	state = g_dfuData.state;

	usbdhs_write(0, &state, sizeof(u8), NULL, NULL);
}

static u8 g_loaderBuf[16384];
static uint g_loaderBufFill;

static u8 g_fpgaBuf[1024];
static uint g_fpgaBufStart; // file offset of first byte in buffer
static uint g_fpgaBufEnd;
static uint g_fpgaBufFill;
static uint g_fpgaBufRPtr;
static uint g_fpgaPushedAddr;
extern unsigned short g_usDataType;

short int ispProcessVME();
void EnableHardware();

void fpgaPushAddr()
{
	g_fpgaPushedAddr = g_fpgaBufRPtr;
}

void fpgaPopAddr()
{
	g_fpgaBufRPtr = g_fpgaPushedAddr;
}

uint8_t fpgaGetByte()
{
	uint8_t res = g_fpgaBuf[g_fpgaBufRPtr - g_fpgaBufStart];
	g_fpgaBufRPtr++;

	return res;
}

static int fpgaFlash(unsigned int offset, const uint8_t* buf, unsigned int len)
{
	int i;

	if(offset == 0) {
		for(i = 0; i < len; i++)
			g_fpgaBuf[i] = buf[i];
		g_fpgaBufStart = 0;
		g_fpgaBufEnd = len;
		g_fpgaBufFill = len;
		g_fpgaBufRPtr = 0;

		// brute force configure GPIOs
		*((volatile u32*)(0x400e0410)) = (1 << 11);
		*((volatile u32*)(0x400e0e00 + 0x44)) = (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8);
		*((volatile u32*)(0x400e0e00 + 0x60)) = (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8);
		*((volatile u32*)(0x400e0e00 + 0x54)) = (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8);
		*((volatile u32*)(0x400e0e00 + 0x10)) = (1 << 5) | (1 << 7) | (1 << 8);
		*((volatile u32*)(0x400e0e00 + 0x00)) = (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8);

		if(fpgaGetByte())
			g_usDataType = COMPRESS;
		else g_usDataType = 0;

		EnableHardware();
	} else {
		for(i = 0; i < len; i++)
			g_fpgaBuf[g_fpgaBufFill + i] = buf[i];
		g_fpgaBufEnd += len;
		g_fpgaBufFill += len;
	}

	while(g_fpgaBufEnd - g_fpgaBufRPtr > 192) {
		if((i = ispProcessVME()) < 0)
			return -1;
	}

	if(g_fpgaBufFill > 384) {
		uint moveby = g_fpgaBufFill - 384;
		uint movelen = g_fpgaBufFill - moveby;

		for(i = 0; i < movelen; i++)
			g_fpgaBuf[i] = g_fpgaBuf[i + moveby];
		g_fpgaBufStart += moveby;
		g_fpgaBufFill -= moveby;
	}

	return 0;
}

int dnloadWrite(u8 altif, uint offset, const u8* buf, uint len)
{
	static const char* partitions[3] = { "Application", "DFU Loader", "FPGA Image" };
	if(altif > 2)
		return DFU_RET_STALL;

	if(offset == 0) {
		dprintf("\nDOWNLOAD: %s partition started (one # every 8192 bytes):\n", partitions[altif]);
	} else if((offset & 8191) == 0) {
		dprintf("#");
	}
	if(len < BOARD_DFU_PAGE_SIZE) {
		if(len > 0)
			dprintf("#");
		dprintf(" - done.\n");
	}

	switch(altif) {
		case 0: // radio application
			if(len > BOARD_DFU_PAGE_SIZE)
				return DFU_RET_STALL;
			if((offset + len) >= (AT91C_IFLASH0_SIZE - 16384))
				return DFU_RET_STALL;
			if(flash_write_bank0(AT91C_IFLASH0 + 16384 + offset, buf, len) < 0)
				return DFU_RET_STALL;
			break;

		case 1: // bootloader
			if(offset == 0)
				g_loaderBufFill = 0;
			if(g_loaderBufFill != offset)
				return DFU_RET_STALL;
			if(g_loaderBufFill + len > 16384)
				return DFU_RET_STALL;
			for(int i = 0; i < len; i++)
				g_loaderBuf[g_loaderBufFill + i] = buf[i];
			g_loaderBufFill += len;
			if((g_loaderBufFill == 16384) || (len < BOARD_DFU_PAGE_SIZE)) {
				dprintf("\nDFU LOADER CHANGED - REBOOTING\n");
				flash_write_loader(g_loaderBuf);
			}
			break;

		case 2: // FPGA image
			if(fpgaFlash(offset, buf, len) < 0) {
				dprintf("FPGA ERROR\n");
				return DFU_RET_STALL;
			}
			break;
	}
	return DFU_RET_ZLP;
}

static void dnloadCB(void *arg, u8 status, uint transferred, uint remaining)
{
	int rc;

	if(status != USBD_STATUS_SUCCESS) {
		usbdhs_stall(0);
		return;
	}

	rc = dnloadWrite(usbDevice.interfaces[0], g_dfuData.totalBytes, (u8*)g_dfuData.buffer, transferred);

	switch(rc) {
		case DFU_RET_ZLP:
			g_dfuData.totalBytes += transferred;
			g_dfuData.state = DFU_STATE_dfuDNLOAD_IDLE;
			usbCtrlSendNull(NULL, 0, 0, 0);
			break;

		case DFU_RET_STALL:
			g_dfuData.state = DFU_STATE_dfuERROR;
			usbdhs_stall(0);
			break;

		case DFU_RET_NOTHING:
			break;
	}
}

static int handleDnload(uint16_t val, uint16_t len, Bool first)
{
	int rc;

	if(len > BOARD_DFU_PAGE_SIZE) {
		g_dfuData.state = DFU_STATE_dfuERROR;
		g_dfuData.status = DFU_STATUS_errADDRESS;
		return DFU_RET_STALL;
	}

	if(first)
		g_dfuData.totalBytes = 0;

	if(len == 0) {
		g_dfuData.state = DFU_STATE_dfuMANIFEST_SYNC;
		return DFU_RET_ZLP;
	}

	/* else: actually read data */
	rc = usbdhs_read(0, g_dfuData.buffer, len, &dnloadCB, NULL);
	if(rc == USBD_STATUS_SUCCESS)
		return DFU_RET_NOTHING;
	else return DFU_RET_STALL;
}

static int handleUpload(uint16_t val, uint16_t len, int first)
{
	return DFU_RET_STALL;
}

static void deviceRequestHandler(const USBGenericRequest* request)
{
    u8 req = usbGenericRequest_GetRequest(request);
    u16 len = usbGenericRequest_GetLength(request);
    u16 val = usbGenericRequest_GetValue(request);
    int ret = DFU_RET_STALL;
    int rc;

	switch(g_dfuData.state) {
		case DFU_STATE_appIDLE:
			switch(req) {
				case USB_REQ_DFU_GETSTATUS:
					sendStatus();
					break;
				case USB_REQ_DFU_GETSTATE:
					sendState();
					break;
				case USB_REQ_DFU_DETACH:
					g_dfuData.state = DFU_STATE_appDETACH;
					ret = DFU_RET_ZLP;
					goto out;
					break;
				default:
					ret = DFU_RET_STALL;
					goto out;
			}
			break;
		case DFU_STATE_appDETACH:
			switch(req) {
				case USB_REQ_DFU_GETSTATUS:
					sendStatus();
					break;
				case USB_REQ_DFU_GETSTATE:
					sendState();
					break;
				default:
					g_dfuData.state = DFU_STATE_appIDLE;
					ret = DFU_RET_STALL;
					goto out;
			}
			/* FIXME: implement timer to return to appIDLE */
			break;
		case DFU_STATE_dfuIDLE:
			switch(req) {
				case USB_REQ_DFU_DNLOAD:
					if(len == 0) {
						g_dfuData.state = DFU_STATE_dfuERROR;
						ret = DFU_RET_STALL;
						goto out;
					}
					g_dfuData.state = DFU_STATE_dfuDNLOAD_SYNC;
					ret = handleDnload(val, len, 1);
					break;
				case USB_REQ_DFU_UPLOAD:
					g_dfuData.state = DFU_STATE_dfuUPLOAD_IDLE;
					handleUpload(val, len, 1);
					break;
				case USB_REQ_DFU_ABORT:
					/* no zlp? */
					ret = DFU_RET_ZLP;
					break;
				case USB_REQ_DFU_GETSTATUS:
					sendStatus();
					break;
				case USB_REQ_DFU_GETSTATE:
					sendState();
					break;
				case USB_REQ_DFU_DETACH:
					g_dfuData.state = DFU_STATE_appDETACH;
					ret = DFU_RET_ZLP;
					goto out;
					break;
				default:
					g_dfuData.state = DFU_STATE_dfuERROR;
					ret = DFU_RET_STALL;
					goto out;
					break;
			}
			break;
		case DFU_STATE_dfuDNLOAD_SYNC:
			switch(req) {
				case USB_REQ_DFU_GETSTATUS:
					sendStatus();
					/* FIXME: state transition depending on block completeness */
					break;
				case USB_REQ_DFU_GETSTATE:
					sendState();
					break;
				default:
					g_dfuData.state = DFU_STATE_dfuERROR;
					ret = DFU_RET_STALL;
					goto out;
			}
			break;
		case DFU_STATE_dfuDNBUSY:
			switch(req) {
				case USB_REQ_DFU_GETSTATUS:
					/* FIXME: only accept getstatus if bwPollTimeout
					 * has elapsed */
					sendStatus();
					break;
				default:
					g_dfuData.state = DFU_STATE_dfuERROR;
					ret = DFU_RET_STALL;
					goto out;
			}
			break;
		case DFU_STATE_dfuDNLOAD_IDLE:
			switch(req) {
				case USB_REQ_DFU_DNLOAD:
					g_dfuData.state = DFU_STATE_dfuDNLOAD_SYNC;
					ret = handleDnload(val, len, 0);
					break;
				case USB_REQ_DFU_ABORT:
					g_dfuData.state = DFU_STATE_dfuIDLE;
					ret = DFU_RET_ZLP;
					break;
				case USB_REQ_DFU_GETSTATUS:
					sendStatus();
					break;
				case USB_REQ_DFU_GETSTATE:
					sendState();
					break;
				default:
					g_dfuData.state = DFU_STATE_dfuERROR;
					ret = DFU_RET_STALL;
					break;
			}
			break;
		case DFU_STATE_dfuMANIFEST_SYNC:
			switch(req) {
				case USB_REQ_DFU_GETSTATUS:
					sendStatus();
					break;
				case USB_REQ_DFU_GETSTATE:
					sendState();
					break;
				default:
					g_dfuData.state = DFU_STATE_dfuERROR;
					ret = DFU_RET_STALL;
					break;
			}
			break;
		case DFU_STATE_dfuMANIFEST:
			switch(req) {
				case USB_REQ_DFU_GETSTATUS:
					/* we don't want to change to WAIT_RST, as it
					 * would mean that we can not support another
					 * DFU transaction before doing the actual
					 * reset.  Instead, we switch to idle and note
					 * that we've already been through MANIFST in
					 * the global variable 'past_manifest'.
					 */
					//g_dfu.state = DFU_STATE_dfuMANIFEST_WAIT_RST;
					g_dfuData.state = DFU_STATE_dfuIDLE;
					g_dfuData.pastManifest = True;
					sendStatus();
					break;
				case USB_REQ_DFU_GETSTATE:
					sendState();
					break;
				default:
					g_dfuData.state = DFU_STATE_dfuERROR;
					ret = DFU_RET_STALL;
					break;
			}
			break;
		case DFU_STATE_dfuMANIFEST_WAIT_RST:
			/* we should never go here */
			break;
		case DFU_STATE_dfuUPLOAD_IDLE:
			switch(req) {
				case USB_REQ_DFU_UPLOAD:
					/* state transition if less data then requested */
					rc = handleUpload(val, len, 0);
					if(rc >= 0 && rc < len)
						g_dfuData.state = DFU_STATE_dfuIDLE;
					break;
				case USB_REQ_DFU_ABORT:
					g_dfuData.state = DFU_STATE_dfuIDLE;
					/* no zlp? */
					ret = DFU_RET_ZLP;
					break;
				case USB_REQ_DFU_GETSTATUS:
					sendStatus();
					break;
				case USB_REQ_DFU_GETSTATE:
					sendState();
					break;
				default:
					g_dfuData.state = DFU_STATE_dfuERROR;
					ret = DFU_RET_STALL;
					break;
			}
			break;
		case DFU_STATE_dfuERROR:
			switch(req) {
				case USB_REQ_DFU_GETSTATUS:
					sendStatus();
					break;
				case USB_REQ_DFU_GETSTATE:
					sendState();
					break;
				case USB_REQ_DFU_CLRSTATUS:
					g_dfuData.state = DFU_STATE_dfuIDLE;
					g_dfuData.status = DFU_STATUS_OK;
					/* no zlp? */
					ret = DFU_RET_ZLP;
					break;
				default:
					g_dfuData.state = DFU_STATE_dfuERROR;
					ret = DFU_RET_STALL;
					break;
			}
			break;
	}

out:
	switch(ret) {
		case DFU_RET_NOTHING:
			break;
		case DFU_RET_ZLP:
			usbdhs_write(0, NULL, 0, NULL, NULL);
			break;
		case DFU_RET_STALL:
			usbdhs_stall(0);
			break;
	}
}

void usbDevice_configure(void)
{
	for(int i = 0; i < BOARD_USB_NUMINTERFACES; i++)
		interfaces[i] = 0;
	g_dfuData.state = DFU_STATE_dfuIDLE;
}

void usbDevice_setSerial(const char* sn)
{
	for(int i = 0; i < 16; i++)
		strSerial[i * 2 + 2] = (u8)*sn++;
}

void usbRequestReceived(const USBGenericRequest* request)
{
	//dprintf("%02x: %02x %04x %04x %04x: ", request->bmRequestType, request->bRequest, request->wValue, request->wIndex, request->wLength);

	/* check for GET_DESCRIPTOR on DFU */
	if(usbGenericRequest_GetType(request) == USBGenericRequest_STANDARD && usbGenericRequest_GetRecipient(request) == USBGenericRequest_DEVICE
	    && usbGenericRequest_GetRequest(request) == USBGenericRequest_GETDESCRIPTOR
	    && usbGetDescriptorRequest_GetDescriptorType(request) == USBFunctionDescriptor_DFU) {
		const USBDeviceDescriptor *deviceDescriptor;
		int terminateWithNull;
		if(usbdhs_isHighSpeed())
			deviceDescriptor = usbDevice.hsDeviceDescriptor;
		else deviceDescriptor = usbDevice.fsDeviceDescriptor;
		terminateWithNull = ((sizeof(USBDFUFunctionDescriptor) % deviceDescriptor->bMaxPacketSize0) == 0);
		usbdhs_write(0, &usbConfigurationDescriptor.function, sizeof(USBDFUFunctionDescriptor), terminateWithNull ? &usbCtrlSendNull : NULL, NULL);
		return;
	}

	if(usbGenericRequest_GetType(request) != USBGenericRequest_CLASS || usbGenericRequest_GetRecipient(request) != USBGenericRequest_INTERFACE) {
		usbGenericRequestHandler(request);
		return;
	}

	deviceRequestHandler(request);
}

void usbDevice_tick(void)
{
	if(g_dfuData.state == DFU_STATE_appDETACH)
		sys_reset();
}

