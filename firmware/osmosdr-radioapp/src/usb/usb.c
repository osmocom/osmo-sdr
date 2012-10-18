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
#include "usb.h"
#include "usbdevice.h"
#include "descriptors.h"
#include "../crt/stdio.h"
#include "../driver/usbdhs.h"
#include "../board.h"

static void setInterface(u8 infnum, u8 setting)
{
	// Make sure alternate settings are supported
	if(usbDevice.interfaces == NULL) {
		usbdhs_stall(0);
	} else {
		// Change the current setting of the interface and trigger the callback
		// if necessary
		if(usbDevice.interfaces[infnum] != setting) {
			usbDevice.interfaces[infnum] = setting;
			//USBDDriverCallbacks_InterfaceSettingChanged(infnum, setting);
		}

		// Acknowledge the request
		usbdhs_write(
			0, // Endpoint #0
			0, // No data buffer
			0, // No data buffer
			(TransferCallback)NULL,
			(void*)NULL);
	}
	}

static void getInterface(u8 infnum)
{
	// Make sure alternate settings are supported, or STALL the control pipe
	if(usbDevice.interfaces == NULL) {
		usbdhs_stall(0);
	} else {
		// Sends the current interface setting to the host
		usbdhs_write(0, &(usbDevice.interfaces[infnum]), 1, NULL, NULL);
	}
}

static void sendDeviceStatus()
{
	static u16 data = 0;
	const USBConfigurationDescriptor* configurationDescriptor;

	// Use different configuration depending on device speed
	if(usbdhs_isHighSpeed())
		configurationDescriptor = usbDevice.hsConfigurationDescriptor;
	else configurationDescriptor = usbDevice.fsConfigurationDescriptor;

	// Check current configuration for power mode (if device is configured)
	if(usbDevice.activeConfiguration != 0) {
		if(usbConfigurationDescriptor_IsSelfPowered(configurationDescriptor))
			data |= 1;
	}

	// Check if remote wake-up is enabled
	if(usbDevice.remoteWakeupEnabled)
		data |= 2;

	// Send the device status
	usbdhs_write(0, &data, 2, NULL, NULL);
}

static void sendEndpointStatus(u8 endpoint)
{
	static u16 data = 0;

	// Check if the endpoint exists
	if(endpoint > CHIP_USB_NUMENDPOINTS) {
		usbdhs_stall(0);
	} else {
		// Check if the endpoint if currently halted
		if(usbdhs_isHalted(endpoint))
			data |= 1;
		// Send the endpoint status
		usbdhs_write(0, &data, 2, NULL, NULL);
	}
}

static void setConfiguration(u8 cfgnum)
{
	USBEndpointDescriptor* endpointDescriptor[CHIP_USB_NUMENDPOINTS + 1];
	const USBConfigurationDescriptor* configurationDescriptor;

	// Use different descriptor depending on device speed
	if(usbdhs_isHighSpeed())
		configurationDescriptor = usbDevice.hsConfigurationDescriptor;
	else configurationDescriptor = usbDevice.fsConfigurationDescriptor;

	// Set & save the desired configuration
	usbdhs_setConfiguration(cfgnum);
	usbDevice.activeConfiguration = cfgnum;

	// If the configuration is not 0, configure endpoints
	if(cfgnum != 0) {
		// Parse configuration to get endpoint descriptors
		usbConfigurationDescriptor_Parse(configurationDescriptor, 0, endpointDescriptor, 0);

		// Configure endpoints
		for(int i = 0; endpointDescriptor[i] != 0; i++)
			usbdhs_configureEndpoint(endpointDescriptor[i]);
	}
	// Should be done before send the ZLP
	//USBDDriverCallbacks_ConfigurationChanged(cfgnum);

	// Acknowledge the request
	usbdhs_write(
		0, // Endpoint #0
	    0, // No data buffer
	    0, // No data buffer
	    (TransferCallback)NULL,
	    (void*)NULL);
}

static void sendDescriptor(u32 type, u32 indexRDesc, u32 length)
{
	const USBDeviceDescriptor* deviceDescriptor;
	const USBConfigurationDescriptor* configurationDescriptor;
	const USBDeviceQualifierDescriptor* deviceQualifierDescriptor;;
	const USBConfigurationDescriptor* otherSpeedDescriptor;
	Bool terminateWithNull = False;
	const USBGenericDescriptor** strings = (const USBGenericDescriptor**)usbDevice.strings;
	const USBGenericDescriptor* string;

	if(usbdhs_isHighSpeed()) {
		//dprintf("HS");
		deviceDescriptor = usbDevice.hsDeviceDescriptor;
		configurationDescriptor = usbDevice.hsConfigurationDescriptor;
		deviceQualifierDescriptor = usbDevice.hsQualifierDescriptor;
		otherSpeedDescriptor = usbDevice.hsOtherSpeedDescriptor;
	} else {
		//dprintf("FS");
		deviceDescriptor = usbDevice.fsDeviceDescriptor;
		configurationDescriptor = usbDevice.fsConfigurationDescriptor;
		deviceQualifierDescriptor = usbDevice.fsQualifierDescriptor;
		otherSpeedDescriptor = usbDevice.fsOtherSpeedDescriptor;
	}

	// Check the descriptor type
	switch(type) {
		case USBGenericDescriptor_DEVICE:
			if(length > usbGenericDescriptor_GetLength((USBGenericDescriptor*)deviceDescriptor))
				length = usbGenericDescriptor_GetLength((USBGenericDescriptor*)deviceDescriptor);
			usbdhs_write(0, deviceDescriptor, length, NULL, NULL);
			break;

		case USBGenericDescriptor_CONFIGURATION:
			// Adjust length and send descriptor
			if(length > usbConfigurationDescriptor_GetTotalLength(configurationDescriptor)) {
				length = usbConfigurationDescriptor_GetTotalLength(configurationDescriptor);
				terminateWithNull = ((length % deviceDescriptor->bMaxPacketSize0) == 0) ? True : False;
			}
			usbdhs_write(0, configurationDescriptor, length, terminateWithNull ? usbCtrlSendNull : NULL, NULL);
			break;

		case USBGenericDescriptor_DEVICEQUALIFIER:
			if(deviceQualifierDescriptor == NULL) {
				usbdhs_stall(0);
			} else {
				if(length > usbGenericDescriptor_GetLength((USBGenericDescriptor*)deviceQualifierDescriptor))
					length = usbGenericDescriptor_GetLength((USBGenericDescriptor*)deviceQualifierDescriptor);
				usbdhs_write(0, deviceQualifierDescriptor, length, NULL, NULL);
			}
			break;

		case USBGenericDescriptor_OTHERSPEEDCONFIGURATION:
			if(otherSpeedDescriptor == NULL) {
				usbdhs_stall(0);
			} else {
				// Adjust length and send descriptor
				if(length > usbConfigurationDescriptor_GetTotalLength(otherSpeedDescriptor)) {
					length = usbConfigurationDescriptor_GetTotalLength(otherSpeedDescriptor);
					terminateWithNull = ((length % deviceDescriptor->bMaxPacketSize0) == 0);
				}
				usbdhs_write(0, otherSpeedDescriptor, length, terminateWithNull ? usbCtrlSendNull : NULL, NULL);
			}
			break;

		case USBGenericDescriptor_STRING:
			// Check if descriptor exists
			if((indexRDesc > usbDevice.numStrings) || (strings[indexRDesc] == NULL)) {
				usbdhs_stall(0);
			} else {
				string = strings[indexRDesc];
				// Adjust length and send descriptor
				if(length > usbGenericDescriptor_GetLength(string)) {
					length = usbGenericDescriptor_GetLength(string);
					terminateWithNull = ((length % deviceDescriptor->bMaxPacketSize0) == 0);
				}
				usbdhs_write(0, string, length, terminateWithNull ? usbCtrlSendNull : NULL, NULL);
			}
			break;

		default:
			usbdhs_stall(0);
			break;
	}
}

void usbGenericRequestHandler(const USBGenericRequest* request)
{
	unsigned char cfgnum;
	unsigned char infnum;
	unsigned char eptnum;
	unsigned char setting;
	u32 type;
	u32 indexDesc;
	u32 length;
	u32 address;

	// Check request code
	switch(usbGenericRequest_GetRequest(request)) {

		case USBGenericRequest_GETDESCRIPTOR:
			// Send the requested descriptor
			type = usbGetDescriptorRequest_GetDescriptorType(request);
			indexDesc = usbGetDescriptorRequest_GetDescriptorIndex(request);
			length = usbGenericRequest_GetLength(request);
			sendDescriptor(type, indexDesc, length);
			break;

		case USBGenericRequest_SETADDRESS:
			// Sends a zero-length packet and then set the device address
			address = usbSetAddressRequest_GetAddress(request);
			usbdhs_write(0, NULL, 0, (TransferCallback)usbdhs_setAddress, (void*)address);
			break;

		case USBGenericRequest_SETCONFIGURATION:
			// Set the requested configuration
			cfgnum = usbSetConfigurationRequest_GetConfiguration(request);
			setConfiguration(cfgnum);
			break;

		case USBGenericRequest_GETSTATUS:
			// Check who is the recipient
			switch(usbGenericRequest_GetRecipient(request)) {
				case USBGenericRequest_DEVICE:
					// Send the device status
					sendDeviceStatus();
					break;

				case USBGenericRequest_ENDPOINT:
					// Send the endpoint status
					eptnum = usbGenericRequest_GetEndpointNumber(request);
					sendEndpointStatus(eptnum);
					break;

				default:
					usbdhs_stall(0);
					break;
			}
			break;

		case USBGenericRequest_SETINTERFACE:
			infnum = usbInterfaceRequest_GetInterface(request);
			setting = usbInterfaceRequest_GetAlternateSetting(request);
			setInterface(infnum, setting);
			break;

		case USBGenericRequest_GETINTERFACE:
			infnum = usbInterfaceRequest_GetInterface(request);
			getInterface(infnum);
			break;


#if 0
			case USBGenericRequest_GETCONFIGURATION:
			TRACE_INFO_WP("gCfg ");

			// Send the current configuration number
			GetConfiguration(pDriver);
			break;

			case USBGenericRequest_CLEARFEATURE:
			TRACE_INFO_WP("cFeat ");

			// Check which is the requested feature
			switch(USBFeatureRequest_GetFeatureSelector(pRequest)) {

				case USBFeatureRequest_ENDPOINTHALT:
				TRACE_INFO_WP("Hlt ");

				// Unhalt endpoint and send a zero-length packet
				USBD_Unhalt(USBGenericRequest_GetEndpointNumber(pRequest));
				USBD_Write(0, 0, 0, 0, 0);
				break;

				case USBFeatureRequest_DEVICEREMOTEWAKEUP:
				TRACE_INFO_WP("RmWU ");

				// Disable remote wake-up and send a zero-length packet
				pDriver->isRemoteWakeUpEnabled = 0;
				USBD_Write(0, 0, 0, 0, 0);
				break;

				default:
				TRACE_WARNING("USBDDriver_RequestHandler: Unknown feature selector (%d)\n\r", USBFeatureRequest_GetFeatureSelector(pRequest));
				USBD_Stall(0);
			}
			break;

			case USBGenericRequest_SETFEATURE:
			TRACE_INFO_WP("sFeat ");

			// Check which is the selected feature
			switch(USBFeatureRequest_GetFeatureSelector(pRequest)) {

				case USBFeatureRequest_DEVICEREMOTEWAKEUP:
				TRACE_INFO_WP("RmWU ");

				// Enable remote wake-up and send a ZLP
				pDriver->isRemoteWakeUpEnabled = 1;
				USBD_Write(0, 0, 0, 0, 0);
				break;

				case USBFeatureRequest_ENDPOINTHALT:
				TRACE_INFO_WP("Halt ");
				// Halt endpoint
				USBD_Halt(USBGenericRequest_GetEndpointNumber(pRequest));
				USBD_Write(0, 0, 0, 0, 0);
				break;

				case USBFeatureRequest_TESTMODE:
				// 7.1.20 Test Mode Support, 9.4.9 SetFeature
				if ((USBGenericRequest_GetRecipient(pRequest) == USBGenericRequest_DEVICE)
					&& ((USBGenericRequest_GetIndex(pRequest) & 0x000F) == 0)) {

					// Handle test request
					USBDDriver_Test(USBFeatureRequest_GetTestSelector(pRequest));
				}
				else {

					USBD_Stall(0);
				}
				break;

				default:
				TRACE_WARNING("USBDDriver_RequestHandler: Unknown feature selector (%d)\n\r", USBFeatureRequest_GetFeatureSelector(pRequest));
				USBD_Stall(0);
			}
			break;

#endif
		default:
			dprintf("USBDDriver_RequestHandler: Unknown request code (%d)\n\r", usbGenericRequest_GetRequest(request));
			usbdhs_stall(0);
			break;
	}
}

void usbCtrlSendNull(void* arg, u8 status, uint transferred, uint remaining)
{
	usbdhs_write(
		0, // Endpoint #0
		0, // No data buffer
		0, // No data buffer
		(TransferCallback)NULL,
		(void*)NULL);
}
