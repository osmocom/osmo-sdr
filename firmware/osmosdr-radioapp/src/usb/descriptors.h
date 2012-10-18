#ifndef INCLUDE_DESCRIPTORS_H
#define INCLUDE_DESCRIPTORS_H

#include "../crt/types.h"

/// The device supports USB 2.00.
#define USBDeviceDescriptor_USB2_00         0x0200


/// Device descriptor type.
#define USBGenericDescriptor_DEVICE                     1
/// Configuration descriptor type.
#define USBGenericDescriptor_CONFIGURATION              2
/// String descriptor type.
#define USBGenericDescriptor_STRING                     3
/// Interface descriptor type.
#define USBGenericDescriptor_INTERFACE                  4
/// Endpoint descriptor type.
#define USBGenericDescriptor_ENDPOINT                   5
/// Device qualifier descriptor type.
#define USBGenericDescriptor_DEVICEQUALIFIER            6
/// Other speed configuration descriptor type.
#define USBGenericDescriptor_OTHERSPEEDCONFIGURATION    7
/// Interface power descriptor type.
#define USBGenericDescriptor_INTERFACEPOWER             8
/// On-The-Go descriptor type.
#define USBGenericDescriptor_OTG                        9
/// Debug descriptor type.
#define USBGenericDescriptor_DEBUG                      10
/// Interface association descriptor type.
#define USBGenericDescriptor_INTERFACEASSOCIATION       11


/// DFU function descriptor
#define USBFunctionDescriptor_DFU                       33


/// Device is self-powered and supports remote wake-up.
#define USBConfigurationDescriptor_SELFPOWERED_RWAKEUP  0xe0


/// Control endpoint type.
#define USBEndpointDescriptor_CONTROL       0
/// Isochronous endpoint type.
#define USBEndpointDescriptor_ISOCHRONOUS   1
/// Bulk endpoint type.
#define USBEndpointDescriptor_BULK          2
/// Interrupt endpoint type.
#define USBEndpointDescriptor_INTERRUPT     3

/// Endpoint receives data from the host.
#define USBEndpointDescriptor_OUT           0
/// Endpoint sends data to the host.
#define USBEndpointDescriptor_IN            1


/// Request is standard.
#define USBGenericRequest_STANDARD              0
/// Request is class-specific.
#define USBGenericRequest_CLASS                 1
/// Request is vendor-specific.
#define USBGenericRequest_VENDOR                2
/// Recipient is the whole device.
#define USBGenericRequest_DEVICE                0
/// Recipient is an interface.
#define USBGenericRequest_INTERFACE             1
/// Recipient is an endpoint.
#define USBGenericRequest_ENDPOINT              2
/// Recipient is another entity.
#define USBGenericRequest_OTHER                 3


/// GET_STATUS request code.
#define USBGenericRequest_GETSTATUS             0
/// CLEAR_FEATURE request code.
#define USBGenericRequest_CLEARFEATURE          1
/// SET_FEATURE request code.
#define USBGenericRequest_SETFEATURE            3
/// SET_ADDRESS request code.
#define USBGenericRequest_SETADDRESS            5
/// GET_DESCRIPTOR request code.
#define USBGenericRequest_GETDESCRIPTOR         6
/// SET_DESCRIPTOR request code.
#define USBGenericRequest_SETDESCRIPTOR         7
/// GET_CONFIGURATION request code.
#define USBGenericRequest_GETCONFIGURATION      8
/// SET_CONFIGURATION request code.
#define USBGenericRequest_SETCONFIGURATION      9
/// GET_INTERFACE request code.
#define USBGenericRequest_GETINTERFACE          10
/// SET_INTERFACE request code.
#define USBGenericRequest_SETINTERFACE          11
/// SYNCH_FRAME request code.
#define USBGenericRequest_SYNCHFRAME            12


#define USBEndpointDescriptor_ADDRESS(direction, number) (((direction & 0x01) << 7) | (number & 0xf))

typedef struct {
	u8 bLength;
	u8 bDescriptorType;
} __attribute__((packed)) USBGenericDescriptor;

typedef struct {
	u8 bLength;
	u8 bDescriptorType;
	u16 bcdUSB;
	u8 bDeviceClass;
	u8 bDeviceSubClass;
	u8 bDeviceProtocol;
	u8 bMaxPacketSize0;
	u16 idVendor;
	u16 idProduct;
	u16 bcdDevice;
	u8 iManufacturer;
	u8 iProduct;
	u8 iSerialNumber;
	u8 bNumConfigurations;
} __attribute__((packed)) USBDeviceDescriptor;

typedef struct {
	u8 bLength;
	u8 bDescriptorType;
	u16 wTotalLength;
	u8 bNumInterfaces;
	u8 bConfigurationValue;
	u8 iConfiguration;
	u8 bmAttributes;
	u8 bMaxPower;
} __attribute__((packed)) USBConfigurationDescriptor;

typedef struct {
	u8 bLength;
	u8 bDescriptorType;
	u16 bcdUSB;
	u8 bDeviceClass;
	u8 bDeviceSubClass;
	u8 bDeviceProtocol;
	u8 bMaxPacketSize0;
	u8 bNumConfigurations;
	u8 bReserved;
} __attribute__((packed)) USBDeviceQualifierDescriptor;

typedef struct {
	u8 bLength;
	u8 bDescriptorType;
	u8 bInterfaceNumber;
	u8 bAlternateSetting;
	u8 bNumEndpoints;
	u8 bInterfaceClass;
	u8 bInterfaceSubClass;
	u8 bInterfaceProtocol;
	u8 iInterface;
} __attribute__((packed)) USBInterfaceDescriptor;

typedef struct {
	u8 bLength;
	u8 bDescriptorType;
	u8 bEndpointAddress;
	u8 bmAttributes;
	u16 wMaxPacketSize;
	u8 bInterval;
} __attribute__((packed)) USBEndpointDescriptor;

typedef struct {
	u8 bmRequestType;
	u8 bRequest;
	u16 wValue;
	u16 wIndex;
	u16 wLength;
} __attribute__((packed)) USBGenericRequest;

// generic descriptor

inline u16 usbGenericDescriptor_GetLength(const USBGenericDescriptor* descriptor)
{
	return descriptor->bLength;
}

inline u8 usbGenericDescriptor_GetType(const USBGenericDescriptor* descriptor)
{
	return descriptor->bDescriptorType;
}

inline USBGenericDescriptor* usbGenericDescriptor_GetNextDescriptor(const USBGenericDescriptor* descriptor)
{
	return (USBGenericDescriptor*)(((u8*)descriptor) + usbGenericDescriptor_GetLength(descriptor));
}

// configuration descriptor

inline uint usbConfigurationDescriptor_GetTotalLength(const USBConfigurationDescriptor* configuration)
{
	return configuration->wTotalLength;
}

inline u8 usbConfigurationDescriptor_IsSelfPowered(const USBConfigurationDescriptor* configuration)
{
	if((configuration->bmAttributes & (1 << 6)) != 0)
		return 1;
	else return 0;
}

void usbConfigurationDescriptor_Parse(
	const USBConfigurationDescriptor* configuration,
	USBInterfaceDescriptor** interfaces,
	USBEndpointDescriptor** endpoints,
	USBGenericDescriptor** others);

// endpoint descriptor

inline u8 usbEndpointDescriptor_GetNumber(const USBEndpointDescriptor* endpoint)
{
	return endpoint->bEndpointAddress & 0x0f;
}

inline u8 usbEndpointDescriptor_GetType(const USBEndpointDescriptor* endpoint)
{
	return endpoint->bmAttributes & 0x03;
}

inline u8 usbEndpointDescriptor_GetDirection(const USBEndpointDescriptor* endpoint)
{
	if((endpoint->bEndpointAddress & 0x80) != 0) {
		return USBEndpointDescriptor_IN;
	} else {
		return USBEndpointDescriptor_OUT;
	}
}

inline u16 usbEndpointDescriptor_GetMaxPacketSize(const USBEndpointDescriptor *endpoint)
{
	return endpoint->wMaxPacketSize;
}

// generic request

inline u8 usbGenericRequest_GetType(const USBGenericRequest* request)
{
	return ((request->bmRequestType >> 5) & 0x3);
}

inline u8 usbGenericRequest_GetRecipient(const USBGenericRequest* request)
{
	return request->bmRequestType & 0xf;
}

inline u8 usbGenericRequest_GetRequest(const USBGenericRequest* request)
{
	return request->bRequest;
}

inline u16 usbGenericRequest_GetValue(const USBGenericRequest* request)
{
	return request->wValue;
}

inline u16 usbGenericRequest_GetLength(const USBGenericRequest* request)
{
	return request->wLength;
}

inline u16 usbGenericRequest_GetIndex(const USBGenericRequest* request)
{
	return request->wIndex;
}

inline u8 usbGenericRequest_GetEndpointNumber(const USBGenericRequest* request)
{
	return usbGenericRequest_GetIndex(request) & 0x0f;
}

// get descriptor request

inline u8 usbGetDescriptorRequest_GetDescriptorType(const USBGenericRequest* request)
{
	return (usbGenericRequest_GetValue(request) >> 8) & 0xff;
}

inline u8 usbGetDescriptorRequest_GetDescriptorIndex(const USBGenericRequest* request)
{
	return usbGenericRequest_GetValue(request) & 0xff;
}

// set address request

inline u8 usbSetAddressRequest_GetAddress(const USBGenericRequest* request)
{
	return usbGenericRequest_GetValue(request) & 0x7f;
}

// set configuration request

inline u8 usbSetConfigurationRequest_GetConfiguration(const USBGenericRequest* request)
{
	return usbGenericRequest_GetValue(request);
}

// interface request

inline u8 usbInterfaceRequest_GetInterface(const USBGenericRequest* request)
{
	return (usbGenericRequest_GetIndex(request) & 0xff);
}

inline u8 usbInterfaceRequest_GetAlternateSetting(const USBGenericRequest* request)
{
	return (usbGenericRequest_GetValue(request) & 0xff);
}

#endif // INCLUDE_DESCRIPTORS_H
