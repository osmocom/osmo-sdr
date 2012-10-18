#ifndef INCLUDE_USBD_H
#define INCLUDE_USBD_H

#include "../crt/types.h"
#include "../usb/descriptors.h"

/// The device is currently suspended.
#define USBD_STATE_SUSPENDED            0
/// USB cable is plugged into the device.
#define USBD_STATE_ATTACHED             1
/// Host is providing +5V through the USB cable.
#define USBD_STATE_POWERED              2
/// Device has been reset.
#define USBD_STATE_DEFAULT              3
/// The device has been given an address on the bus.
#define USBD_STATE_ADDRESS              4
/// A valid configuration has been selected.
#define USBD_STATE_CONFIGURED           5

/// Indicates the operation was successful.
#define USBD_STATUS_SUCCESS             0
/// Endpoint/device is already busy.
#define USBD_STATUS_LOCKED              1
/// Operation has been aborted.
#define USBD_STATUS_ABORTED             2
/// Operation has been aborted because the device has been reset.
#define USBD_STATUS_RESET               3
/// Part ot operation successfully done.
#define USBD_STATUS_PARTIAL_DONE        4
/// Operation failed because parameter error
#define USBD_STATUS_INVALID_PARAMETER   5
/// Operation failed because in unexpected state
#define USBD_STATUS_WRONG_STATE         6
/// Operation failed because HW not supported
#define USBD_STATUS_HW_NOT_SUPPORTED    0xFE

typedef void (*TransferCallback)(void *pArg, unsigned char status, unsigned int transferred, unsigned int remaining);

void usbdhs_configure(void);
void usbdhs_connect(void);
void usbdhs_setConfiguration(uint cfgnum);
void usbdhs_configureEndpoint(const USBEndpointDescriptor* pDescriptor);
char usbdhs_write(unsigned char bEndpoint, const void *pData, unsigned int dLength, TransferCallback fCallback, void *pArgument);
char usbdhs_read(unsigned char bEndpoint, void *pData, unsigned int dLength, TransferCallback fCallback, void *pArgument);
unsigned char usbdhs_stall(uint endpoint);
Bool usbdhs_isHighSpeed(void);
Bool usbdhs_isHalted(uint endpoint);
void usbdhs_setAddress(uint address);

#endif // INCLUDE_USBD_H
