/* (C) 2011-2012 by Harald Welte <laforge@gnumonks.org>
 * (C) 2011-2012 by Christian Daniel <cd@maintech.de>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include "../crt/stdio.h"
#include "../board.h"
#include "../driver/usbdhs.h"
#include "../driver/sys.h"
#include "../driver/flash.h"
#include "usbdevice.h"
#include "usb.h"
#include "../components.h"
#include "../driver/sdrssc.h"
#include "../driver/sdrmci.h"
#include "../driver/sdrfpga.h"
#include "../driver/e4k.h"
#include "../driver/chanfilter.h"
#include "../driver/mci.h"

#define STR_MANUFACTURER 1
#define STR_PRODUCT 2
#define STR_SERIAL 3
#define STR_CONFIG 4
#define NUM_STRINGS 4

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
	// ................
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
	// OsmoSDR Radio Application
	USBStringDescriptor_LENGTH(25),
	USBGenericDescriptor_STRING,
	USBStringDescriptor_UNICODE('O'),
	USBStringDescriptor_UNICODE('s'),
	USBStringDescriptor_UNICODE('m'),
	USBStringDescriptor_UNICODE('o'),
	USBStringDescriptor_UNICODE('S'),
	USBStringDescriptor_UNICODE('D'),
	USBStringDescriptor_UNICODE('R'),
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
	USBStringDescriptor_UNICODE('n'),
};

static const u8* strings[NUM_STRINGS + 1] = {
	languageIdDescriptor,
	strManufacturer,
	strProduct,
	strSerial,
	strConfig
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

#define SDRConfigurationDescriptors_DATAIN 6

typedef struct {
	USBConfigurationDescriptor configuration;
	USBInterfaceDescriptor interface;
	USBEndpointDescriptor endpoint;
} __attribute__((packed)) OsmoSDRConfigurationDescriptor;

static const OsmoSDRConfigurationDescriptor osmoSDRConfigurationDescriptor = {
	.configuration = {
		.bLength = sizeof(USBConfigurationDescriptor),
		.bDescriptorType = USBGenericDescriptor_CONFIGURATION,
		.wTotalLength = sizeof(USBConfigurationDescriptor) + sizeof(USBInterfaceDescriptor) + sizeof(USBEndpointDescriptor),
		.bNumInterfaces = BOARD_USB_NUMINTERFACES,
		.bConfigurationValue = 1,
		.iConfiguration = STR_CONFIG,
		.bmAttributes = BOARD_USB_BMATTRIBUTES,
		.bMaxPower = 250
	},
	.interface = {
		.bLength = sizeof(USBInterfaceDescriptor),
		.bDescriptorType = USBGenericDescriptor_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 1,
		.bInterfaceClass = 0xfe,
		.bInterfaceSubClass = 0,
		.iInterface = 0,
		.bInterfaceProtocol = 0
	},
	.endpoint = {
		.bLength = sizeof(USBEndpointDescriptor),
		.bDescriptorType = USBGenericDescriptor_ENDPOINT,
		USBEndpointDescriptor_ADDRESS(USBEndpointDescriptor_IN, SDRConfigurationDescriptors_DATAIN),
		USBEndpointDescriptor_BULK,
		0x200, // packet size
		0 // Polling interval = 0 ms
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
	.fsConfigurationDescriptor = (const USBConfigurationDescriptor*)&osmoSDRConfigurationDescriptor.configuration,
	.fsQualifierDescriptor = &usbDeviceQualifierDescriptor,
	.fsOtherSpeedDescriptor = (const USBConfigurationDescriptor*)&osmoSDRConfigurationDescriptor.configuration,

	.hsDeviceDescriptor = &usbDeviceDescriptor,
	.hsConfigurationDescriptor = (const USBConfigurationDescriptor*)&osmoSDRConfigurationDescriptor.configuration,
	.hsQualifierDescriptor = &usbDeviceQualifierDescriptor,
	.hsOtherSpeedDescriptor = (const USBConfigurationDescriptor*)&osmoSDRConfigurationDescriptor.configuration,

	.strings = strings,
	.numStrings = NUM_STRINGS,

	.activeConfiguration = 0,
	.remoteWakeupEnabled = False,
	.interfaces = interfaces
};

#define OSMOSDR_CTRL_WRITE 0x07
#define OSMOSDR_CTRL_READ 0x87

typedef struct {
	u16 func;
	u16 len;
	const char* name;
} Request;

typedef struct {
	u8 data[16];
	u16 func;
} WriteState;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define FUNC(group, function) ((group << 8) | function)

#define GROUP_GENERAL 0x00
#define GROUP_FPGA_V2 0x01
#define GROUP_VCXO_SI570 0x02
#define GROUP_TUNER_E4K 0x03
#define GROUP_FILTER_V1 0x04

const static Request g_writeRequests[] = {
	// general api
	{ FUNC(GROUP_GENERAL, 0x00), 0, "general init" }, // init whatever
	{ FUNC(GROUP_GENERAL, 0x01), 0, "general power down" }, // power down
	{ FUNC(GROUP_GENERAL, 0x02), 0, "general power up" }, // power up
	{ FUNC(GROUP_GENERAL, 0x03), 0, "switch to DFU" }, // switch to DFU mode

	// fpga commands
	{ FUNC(GROUP_FPGA_V2, 0x00), 0, "FPGA init" }, // fpga init
	{ FUNC(GROUP_FPGA_V2, 0x01), 5, "FPGA register write" }, // osdr_fpga_reg_write(uint8_t reg, uint32_t val)
	{ FUNC(GROUP_FPGA_V2, 0x02), 1, "FPGA set decimation" }, // osdr_fpga_set_decimation(uint8_t val)
	{ FUNC(GROUP_FPGA_V2, 0x03), 1, "FPGA set I/Q swap" }, // osdr_fpga_set_iq_swap(uint8_t val)
	{ FUNC(GROUP_FPGA_V2, 0x04), 4, "FPGA set I/Q gain" }, // osdr_fpga_set_iq_gain(uint16_t igain, uint16_t qgain)
	{ FUNC(GROUP_FPGA_V2, 0x05), 4, "FPGA set I/Q offset" }, // osdr_fpga_set_iq_ofs(int16_t iofs, int16_t qofs)

	// si570 vcxo commads
	{ FUNC(GROUP_VCXO_SI570, 0x00), 0, "SI570 init" }, // si570_init()
	{ FUNC(GROUP_VCXO_SI570, 0x01), 16, "Si570 register write" }, // si570_reg_write
	{ FUNC(GROUP_VCXO_SI570, 0x02), 8, "SI570 set frequency and trim value" }, // si570_set_freq(uint32_t freq, int trim);
	{ FUNC(GROUP_VCXO_SI570, 0x03), 4, "SI570 set frequency" }, // si570_set_freq(uint32_t freq, int trim);

	// e4000 tuner commands
	{ FUNC(GROUP_TUNER_E4K, 0x00), 0, "E4K init" }, // e4k_init()
	{ FUNC(GROUP_TUNER_E4K, 0x01), 2, "E4K register write" }, // reg write
	{ FUNC(GROUP_TUNER_E4K, 0x02), 5, "E4K set if gain" }, // e4k_if_gain_set(uint8_t stage, int8_t value)
	{ FUNC(GROUP_TUNER_E4K, 0x03), 1, "E4K set mixer gain" }, // e4k_mixer_gain_set(struct e4k_state *e4k, int8_t value)
	{ FUNC(GROUP_TUNER_E4K, 0x04), 1, "E4K set commonmode" }, // e4k_commonmode_set(int8_t value)
	{ FUNC(GROUP_TUNER_E4K, 0x05), 4, "E4K tune" }, // e4k_tune_freq(uint32_t freq)
	{ FUNC(GROUP_TUNER_E4K, 0x06), 5, "E4K set filter bandwidth" }, // e4k_if_filter_bw_set(enum e4k_if_filter filter, uint32_t bandwidth)
	{ FUNC(GROUP_TUNER_E4K, 0x07), 1, "E4K set channel filter" }, // e4k_if_filter_chan_enable(int on)
	{ FUNC(GROUP_TUNER_E4K, 0x08), 2, "E4K set DC offset" }, // e4k_manual_dc_offset(int8_t iofs, int8_t qofs)
	{ FUNC(GROUP_TUNER_E4K, 0x09), 0, "E4K DC offset calibrate" }, // e4k_dc_offset_calibrate()
	{ FUNC(GROUP_TUNER_E4K, 0x0a), 0, "E4K DC offset generate table" }, // e4k_dc_offset_gen_table()
	{ FUNC(GROUP_TUNER_E4K, 0x0b), 4, "E4K set LNA gain" }, // e4k_set_lna_gain(int32_t gain)
	{ FUNC(GROUP_TUNER_E4K, 0x0c), 1, "E4K set manual LNA gain" }, // e4k_enable_manual_gain(uint8_t manual)
	{ FUNC(GROUP_TUNER_E4K, 0x0d), 4, "E4K set enhanced gain" }, // e4k_set_enh_gain(int32_t gain)

	// op-amp based channel filter
	{ FUNC(GROUP_FILTER_V1, 0x00), 0, "filter init" }, // filter init
	{ FUNC(GROUP_FILTER_V1, 0x01), 3, "filter register write" }, // reg write
	{ FUNC(GROUP_FILTER_V1, 0x02), 4, "filter set all" }, // write all four potentiometers
};

static WriteState g_writeState;

static u32 read_bytewise32(const u8* data)
{
	return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static u16 read_bytewise16(const u8* data)
{
	return (data[0] << 8) | data[1];
}

static void sourceStart(void);

static void sourceComplete(void* arg, u8 status, uint transferred, uint remaining)
{
	usbDevice.running = False;
#if defined(BOARD_SAMPLE_SOURCE_SSC)
	sdrssc_returnBuffer((DMABuffer*)arg);
#endif // defined(BOARD_SAMPLE_SOURCE_SSC)
#if defined(BOARD_SAMPLE_SOURCE_MCI)
	sdrmci_returnBuffer((DMABuffer*)arg);
#endif // defined(BOARD_SAMPLE_SOURCE_MCI)

	if((status != 0) || (remaining != 0)) {
		return;
	} else {
		sourceStart();
	}
}

static void sourceStart(void)
{
	if(usbDevice.running)
		return;

	DMABuffer* buffer = dmaBuffer_dequeue(&usbDevice.queue);
	if(buffer != NULL) {
		usbDevice.running = True;
		if(usbdhs_write(SDRConfigurationDescriptors_DATAIN, buffer->data, buffer->size, sourceComplete, buffer) != USBD_STATUS_SUCCESS) {
			usbDevice.running = False;
#if defined(BOARD_SAMPLE_SOURCE_SSC)
			sdrssc_returnBuffer(buffer);
#endif // defined(BOARD_SAMPLE_SOURCE_SSC)
#if defined(BOARD_SAMPLE_SOURCE_MCI)
			sdrmci_returnBuffer(buffer);
#endif // defined(BOARD_SAMPLE_SOURCE_MCI)
			return;
		}
	} else {
		usbDevice.running = False;
	}
}

static void finalizeWrite(void* arg, u8 status, uint transferred, uint remaining)
{
	int res = 0;

	if((status != 0) ||(remaining != 0)) {
		dprintf("USB request failed\n");
		usbdhs_stall(0);
		return;
	}

	switch(g_writeState.func) {
		// general api
		case FUNC(GROUP_GENERAL, 0x00): // init all
			// no op so far
			break;
		case FUNC(GROUP_GENERAL, 0x01): // power down
			sdrfpga_setPower(False);
			e4k_setPower(&g_e4kCtx, False);
			break;
		case FUNC(GROUP_GENERAL, 0x02): // power up
			sdrfpga_setPower(True);
			e4k_setPower(&g_e4kCtx, True);
			break;
        case FUNC(GROUP_GENERAL, 0x03): // switch to DFU
			sys_reset(True);
			break;


		// fpga commands
		case FUNC(GROUP_FPGA_V2, 0x00): // fpga init
			// no op so far
			break;
		case FUNC(GROUP_FPGA_V2, 0x01): // reg write
			sdrfpga_regWrite(g_writeState.data[0], read_bytewise32(g_writeState.data + 1));
			break;
		case FUNC(GROUP_FPGA_V2, 0x02): // set decimation
			sdrfpga_setDecimation(g_writeState.data[0]);
#if defined(BOARD_SAMPLE_SOURCE_MCI)
			mci_setSpeed(AT91C_BASE_MCI0, g_writeState.data[0]);
#endif // defined(BOARD_SAMPLE_SOURCE_MCI)
			break;
		case FUNC(GROUP_FPGA_V2, 0x03): // set IQ swap
			sdrfpga_setIQSwap(g_writeState.data[0]);
			break;
		case FUNC(GROUP_FPGA_V2, 0x04): // set IQ gain
			sdrfpga_setIQGain(read_bytewise16(g_writeState.data), read_bytewise16(g_writeState.data + 2));
			break;
		case FUNC(GROUP_FPGA_V2, 0x05): // set IQ offset
			sdrfpga_setIQOfs(read_bytewise16(g_writeState.data), read_bytewise16(g_writeState.data + 2));
			break;

		// si570 vcxo commands
		case FUNC(GROUP_VCXO_SI570, 0x00): // si570_init()
			res = si570_init(&g_si570Ctx);
			break;
		case FUNC(GROUP_VCXO_SI570, 0x01): // reg write
			res = si570_regWrite(&g_si570Ctx, g_writeState.data[0], g_writeState.data[1], g_writeState.data + 2);
			break;
		case FUNC(GROUP_VCXO_SI570, 0x02): // set frequency and trim value
			res = si570_setFrequencyAndTrim(&g_si570Ctx, read_bytewise32(g_writeState.data), read_bytewise32(g_writeState.data + 4));
			break;
		case FUNC(GROUP_VCXO_SI570, 0x03): // set frequency
			res = si570_setFrequency(&g_si570Ctx, read_bytewise32(g_writeState.data));
			break;

		// e4000 tuner commands
		case FUNC(GROUP_TUNER_E4K, 0x00): // e4k_reInit
			res = e4k_reInit(&g_e4kCtx);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x01): // reg write
			res = e4k_setReg(&g_e4kCtx, g_writeState.data[0], g_writeState.data[1]);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x02): // set IF gain
			res = e4k_ifGainSet(&g_e4kCtx, g_writeState.data[0], read_bytewise32(g_writeState.data + 1));
			e4k_dcOffsetCalibrate(&g_e4kCtx);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x03): // set mixer gain
			res = e4k_mixerGainSet(&g_e4kCtx, g_writeState.data[0]);
			e4k_dcOffsetCalibrate(&g_e4kCtx);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x04): // set common mode
			res = e4k_commonModeSet(&g_e4kCtx, g_writeState.data[0]);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x05): // tune
			res = e4k_tune(&g_e4kCtx, read_bytewise32(g_writeState.data) / 1000);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x06): // set if filter BW
			res = e4k_ifFilterBWSet(&g_e4kCtx, g_writeState.data[0], read_bytewise32(g_writeState.data + 1));
			break;
		case FUNC(GROUP_TUNER_E4K, 0x07): // set channel filter
			res = e4k_ifFilterChanEnable(&g_e4kCtx, g_writeState.data[0]);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x08): // set DC offset
			res = e4k_manualDCOffsetSet(&g_e4kCtx,
				g_writeState.data[0] & 0x3f, (g_writeState.data[0] >> 6) & 3,
				g_writeState.data[1] & 0x3f, (g_writeState.data[1] >> 6) & 3);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x09): // calibrate DC offset
			res = e4k_dcOffsetCalibrate(&g_e4kCtx);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x0a): // generate DC offset table
			res = e4k_dcOffsetGenTable(&g_e4kCtx);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x0b): // set LNA gain
			res = e4k_setLNAGain(&g_e4kCtx, (s32)read_bytewise32(g_writeState.data));
			break;
		case FUNC(GROUP_TUNER_E4K, 0x0c): // enable manual gain control
			res = e4k_enableManualGain(&g_e4kCtx, g_writeState.data[0]);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x0d): // enable enhanced gain
			res = e4k_setEnhGain(&g_e4kCtx, read_bytewise32(g_writeState.data));
			break;

		// op-amp based channel filter
		case FUNC(GROUP_FILTER_V1, 0x00): // init
			// no op so far
			break;
		case FUNC(GROUP_FILTER_V1, 0x01): // reg write
			res = chanfilter_regWrite(g_writeState.data[0], g_writeState.data[1], g_writeState.data[2]);
			break;
		case FUNC(GROUP_FILTER_V1, 0x02): // set all potis
			res = chanfilter_setAll(g_writeState.data[0], g_writeState.data[1], g_writeState.data[2], g_writeState.data[3]);
			break;

		default:
			res = -1;
			break;
	}

	if(res >= 0) {
		dprintf("ok\n");
		usbdhs_write(0, NULL, 0, NULL, NULL);
	} else {
		dprintf("error\n");
		usbdhs_stall(0);
	}
}

static void osmosdrWrite(const USBGenericRequest* request)
{
	u16 func = usbGenericRequest_GetValue(request);
	int len = usbGenericRequest_GetLength(request);
	int i;

	dprintf("%02x: %02x %04x %04x %04x: ", request->bmRequestType, request->bRequest, request->wValue, request->wIndex, request->wLength);

	for(i = 0; i < ARRAY_SIZE(g_writeRequests); i++) {
		if(g_writeRequests[i].func == func)
			break;
	}
	if(i == ARRAY_SIZE(g_writeRequests)) {
		usbdhs_stall(0);
		dprintf("OsmoSDR unknown request\n");
		return;
	}
	if(len != g_writeRequests[i].len) {
		usbdhs_stall(0);
		dprintf("OsmoSDR invalid request size (%d != %d)\n", len, g_writeRequests[i].len);
		return;
	}

	dprintf("%s: ", g_writeRequests[i].name);
	g_writeState.func = func;

	if(len > 0)
		usbdhs_read(0, g_writeState.data, len, finalizeWrite, 0);
	else finalizeWrite(NULL, 0, 0, 0);
}

void usbDevice_configure(void)
{
	for(int i = 0; i < BOARD_USB_NUMINTERFACES; i++)
		interfaces[i] = 0;
	dmaBuffer_init(&usbDevice.queue);
	usbDevice.running = False;
}

void usbDevice_setSerial(const char* sn)
{
	for(int i = 0; i < 16; i++)
		strSerial[i * 2 + 2] = (u8)*sn++;
}

void usbDevice_requestReceived(const USBGenericRequest* request)
{
	if((usbGenericRequest_GetType(request) == USBGenericRequest_VENDOR) &&
		(usbGenericRequest_GetRecipient(request) == USBGenericRequest_DEVICE) &&
		(usbGenericRequest_GetRequest(request) == OSMOSDR_CTRL_WRITE)) {
		osmosdrWrite(request);
		return;
	}

	if((usbGenericRequest_GetType(request) != USBGenericRequest_CLASS) ||
		(usbGenericRequest_GetRecipient(request) != USBGenericRequest_INTERFACE)) {
		dprintf("%02x: %02x %04x %04x %04x: ", request->bmRequestType, request->bRequest, request->wValue, request->wIndex, request->wLength);
		dprintf("USB generic\n");
		usbGenericRequestHandler(request);
		return;
	}

	usbdhs_stall(0);
}

void usbDevice_submitBuffer(DMABuffer* buffer)
{
	dmaBuffer_enqueue(&usbDevice.queue, buffer);
	sourceStart();
}

void usbDevice_tick(void)
{
}
