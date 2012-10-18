#ifndef INCLUDE_USB_H
#define INCLUDE_USB_H

#include "usb.h"
#include "descriptors.h"

void usbGenericRequestHandler(const USBGenericRequest* request);
void usbCtrlSendNull(void* arg, u8 status, uint transferred, uint remaining);

#endif // INCLUDE_USB_H
