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

#include "descriptors.h"

void usbConfigurationDescriptor_Parse(
	const USBConfigurationDescriptor* configuration,
	USBInterfaceDescriptor** interfaces,
	USBEndpointDescriptor** endpoints,
	USBGenericDescriptor** others)
{
	// Get size of configuration to parse
	int size = usbConfigurationDescriptor_GetTotalLength(configuration);
	size -= sizeof(USBConfigurationDescriptor);

	// Start parsing descriptors
	USBGenericDescriptor *descriptor = (USBGenericDescriptor *)configuration;
	while(size > 0) {
		// Get next descriptor
		descriptor = usbGenericDescriptor_GetNextDescriptor(descriptor);
		size -= usbGenericDescriptor_GetLength(descriptor);

		// Store descriptor in correponding array
		if(usbGenericDescriptor_GetType(descriptor) == USBGenericDescriptor_INTERFACE) {
			if(interfaces) {
				*interfaces = (USBInterfaceDescriptor*)descriptor;
				interfaces++;
			}
		} else if(usbGenericDescriptor_GetType(descriptor) == USBGenericDescriptor_ENDPOINT) {
			if(endpoints) {
				*endpoints = (USBEndpointDescriptor *)descriptor;
				endpoints++;
			}
		} else if(others) {
			*others = descriptor;
			others++;
		}
	}

	// Null-terminate arrays
	if(interfaces) {
		*interfaces = 0;
	}
	if(endpoints) {
		*endpoints = 0;
	}
	if(others) {
		*others = 0;
	}
}
