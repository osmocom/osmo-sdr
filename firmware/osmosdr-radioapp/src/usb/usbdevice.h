#ifndef INCLUDE_USBDEVICE_H
#define INCLUDE_USBDEVICE_H

#include "../crt/types.h"
#include "usb.h"
#include "../driver/dmabuffer.h"

typedef struct USBDevice_ {
	const USBDeviceDescriptor* fsDeviceDescriptor;
	const USBConfigurationDescriptor* fsConfigurationDescriptor;
	const USBDeviceQualifierDescriptor* fsQualifierDescriptor;
	const USBConfigurationDescriptor* fsOtherSpeedDescriptor;

	const USBDeviceDescriptor* hsDeviceDescriptor;
	const USBConfigurationDescriptor* hsConfigurationDescriptor;
	const USBDeviceQualifierDescriptor* hsQualifierDescriptor;
	const USBConfigurationDescriptor* hsOtherSpeedDescriptor;

	const u8** strings;
	uint numStrings;

	uint activeConfiguration;
	Bool remoteWakeupEnabled;
	u8* interfaces;

	DMABufferQueue queue;
	Bool running;
} USBDevice;

extern USBDevice usbDevice;

void usbDevice_configure(void);
void usbDevice_setSerial(const char* sn);

void usbDevice_requestReceived(const USBGenericRequest* request);
void usbDevice_submitBuffer(DMABuffer* buffer);

void usbDevice_tick(void);

#endif // INCLUDE_USBDEVICE_H
