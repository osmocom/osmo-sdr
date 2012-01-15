#include <board.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#include <memories/flash/flashd.h>

#include <usb/common/core/USBStringDescriptor.h>

#include <usb/device/dfu/dfu.h>

/* 128bit binary serial, equals 16 bytes -> 32 hex digits */
static uint8_t usb_serial_string[USBStringDescriptor_LENGTH(32)];

/* convert from 7-bit ASCII to USB string */
static int to_usb_string(unsigned char *out, int out_len, const char *in)
{
	int in_len = strlen(in);
	int num_out = USBStringDescriptor_LENGTH(in_len);
	int i;
	unsigned char *cur = out;

	if (num_out > out_len || num_out >= 255 || num_out < 0)
		return -EINVAL;

	*cur++ = num_out;
	*cur++ = USBGenericDescriptor_STRING;

	for (i = 0; i < in_len; i++) {
		*cur++ = in[i];
		*cur++ = 0;
	}

	return cur - out;
}

static int chip_uid_to_usbstring(void)
{
	unsigned char uniqueID[17];
	int rc;

	memset(uniqueID, 0, sizeof(uniqueID));

	FLASHD_Initialize(0);
	rc = FLASHD_ReadUniqueID((unsigned long *) uniqueID);
	if (rc != 0)
		return -EIO;

	/* we skip the step to transform the unique-id into hex before
	 * converting it into a USB string, as it seems to consist of ASCII
	 * characters instead of a true 128 bit binary unique identifier */
	rc = to_usb_string(usb_serial_string, sizeof(usb_serial_string), (char *) uniqueID);
	if (rc > 0)
		return 0;

	return rc;
}

int chipid_to_usbserial(void)
{
	int rc;

	rc = chip_uid_to_usbstring();
	if (rc < 0)
		return rc;

	set_usb_serial_str(usb_serial_string);

	return 0;
}
