/*
 * Copyright (C) 2012 by Dimitri Stolnikov <horiz0n@gmx.net>
 * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifndef _WIN32
#include <unistd.h>
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#include <libusb.h>

/*
 * All libusb callback functions should be marked with the LIBUSB_CALL macro
 * to ensure that they are compiled with the same calling convention as libusb.
 *
 * If the macro isn't available in older libusb versions, we simply define it.
 */
#ifndef LIBUSB_CALL
#define LIBUSB_CALL
#endif

#include "osmosdr.h"

typedef struct osmosdr_tuner {
	/* tuner interface */
	int (*init)(void *);
	int (*exit)(void *);
	int (*set_freq)(void *, uint32_t freq /* Hz */);
	int (*set_bw)(void *, int bw /* Hz */);
	int (*set_gain)(void *, int gain /* dB */);
	int (*set_gain_mode)(void *, int manual);
} osmosdr_tuner_t;

enum osmosdr_async_status {
	OSMOSDR_INACTIVE = 0,
	OSMOSDR_CANCELING,
	OSMOSDR_RUNNING
};

struct osmosdr_dev {
	libusb_context *ctx;
	struct libusb_device_handle *devh;
	uint32_t xfer_buf_num;
	uint32_t xfer_buf_len;
	struct libusb_transfer **xfer;
	unsigned char **xfer_buf;
	osmosdr_read_async_cb_t cb;
	void *cb_ctx;
	enum osmosdr_async_status async_status;
	/* adc context */
	uint32_t rate; /* Hz */
	uint32_t adc_clock; /* Hz */
	/* tuner context */
	osmosdr_tuner_t *tuner;
	uint32_t tun_clock; /* Hz */
	uint32_t freq; /* Hz */
	int gain; /* dB */
};

typedef struct osmosdr_dongle {
	uint16_t vid;
	uint16_t pid;
	const char *name;
} osmosdr_dongle_t;

static osmosdr_dongle_t known_devices[] = {
	{ 0x16c0, 0x0763, "sysmocom OsmoSDR" },
	/* here come future modifications of the OsmoSDR */
};

#define DEFAULT_BUF_NUMBER	32
#define DEFAULT_BUF_LENGTH	(16 * 32 * 512)

// TODO: change the constants according to the limits imposed by the hardware

#define DEF_ADC_FREQ	28800000
#define MIN_ADC_FREQ	(DEF_ADC_FREQ - 1000)
#define MAX_ADC_FREQ	(DEF_ADC_FREQ + 1000)

#define DEF_E4K_FREQ	28800000
#define MIN_E4K_FREQ	(DEF_E4K_FREQ - 1000)
#define MAX_E4K_FREQ	(DEF_E4K_FREQ + 1000)

#define MAX_SAMP_RATE	3200000

#define CTRL_IN		(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN)
#define CTRL_OUT	(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)
#define CTRL_TIMEOUT	300
#define BULK_TIMEOUT	0

int osmosdr_set_xtal_freq(osmosdr_dev_t *dev, uint32_t adc_clock, uint32_t tun_clock)
{
	int r = 0;

	if (!dev)
		return -1;

	if (adc_clock > 0 &&
		(adc_clock < MIN_ADC_FREQ || adc_clock > MAX_ADC_FREQ))
		return -2;

	if (dev->adc_clock != adc_clock) {
		if (0 == adc_clock)
			adc_clock = DEF_ADC_FREQ;

		dev->adc_clock = adc_clock;

		/* update xtal-dependent settings */
		if (dev->rate)
			r = osmosdr_set_sample_rate(dev, dev->rate);
	}

	if (dev->tun_clock != tun_clock) {
		if (0 == tun_clock)
			tun_clock = dev->adc_clock;

		dev->tun_clock = tun_clock;

		/* update xtal-dependent settings */
		if (dev->freq)
			r = osmosdr_set_center_freq(dev, dev->freq);
	}

	return r;
}

int osmosdr_get_xtal_freq(osmosdr_dev_t *dev, uint32_t *adc_clock, uint32_t *tun_clock)
{
	if (!dev)
		return -1;

	*adc_clock = dev->adc_clock;

	if (!dev->tuner)
		return -2;

	*tun_clock = dev->tun_clock;

	return 0;
}

int osmosdr_get_usb_strings(osmosdr_dev_t *dev, char *manufact, char *product,
			    char *serial)
{
	struct libusb_device_descriptor dd;
	libusb_device *device = NULL;
	const int buf_max = 256;
	int r = 0;

	if (!dev || !dev->devh)
		return -1;

	device = libusb_get_device(dev->devh);

	r = libusb_get_device_descriptor(device, &dd);
	if (r < 0)
		return -1;

	if (manufact) {
		memset(manufact, 0, buf_max);
		libusb_get_string_descriptor_ascii(dev->devh, dd.iManufacturer,
						   (unsigned char *)manufact,
						   buf_max);
	}

	if (product) {
		memset(product, 0, buf_max);
		libusb_get_string_descriptor_ascii(dev->devh, dd.iProduct,
						   (unsigned char *)product,
						   buf_max);
	}

	if (serial) {
		memset(serial, 0, buf_max);
		libusb_get_string_descriptor_ascii(dev->devh, dd.iSerialNumber,
						   (unsigned char *)serial,
						   buf_max);
	}

	return 0;
}

int osmosdr_set_center_freq(osmosdr_dev_t *dev, uint32_t freq)
{
	int r = -1;
	double f = (double) freq;

	if (!dev || !dev->tuner)
		return -1;

	if (dev->tuner->set_freq) {
		/* TODO: r = dev->tuner->set_freq(dev, freq); */
	}

	if (!r)
		dev->freq = freq;
	else
		dev->freq = 0;

	return r;
}

uint32_t osmosdr_get_center_freq(osmosdr_dev_t *dev)
{
	if (!dev || !dev->tuner)
		return 0;

	return dev->freq;
}

int osmosdr_set_tuner_gain(osmosdr_dev_t *dev, int gain)
{
	int r = 0;

	if (!dev || !dev->tuner)
		return -1;

	if (dev->tuner->set_gain) {
		/* TODO: r = dev->tuner->set_gain((void *)dev, gain); */
	}

	if (!r)
		dev->gain = gain;
	else
		dev->gain = 0;

	return r;
}

int osmosdr_get_tuner_gain(osmosdr_dev_t *dev)
{
	if (!dev || !dev->tuner)
		return 0;

	return dev->gain;
}

int osmosdr_set_tuner_gain_mode(osmosdr_dev_t *dev, int mode)
{
	int r = 0;

	if (!dev || !dev->tuner)
		return -1;

	if (dev->tuner->set_gain_mode) {
		/* TODO: r = dev->tuner->set_gain_mode((void *)dev, mode); */
	}

	return r;
}

int osmosdr_set_sample_rate(osmosdr_dev_t *dev, uint32_t samp_rate)
{
	uint16_t r = 0;

	if (!dev)
		return -1;

	/* check for the maximum rate the resampler supports */
	if (samp_rate > MAX_SAMP_RATE)
		samp_rate = MAX_SAMP_RATE;

	if (dev->tuner && dev->tuner->set_bw) {
		/* TODO: dev->tuner->set_bw(dev, samp_rate); */
	}

	if (!r)
		dev->rate = samp_rate;
	else
		dev->rate = 0;

	return 0;
}

uint32_t osmosdr_get_sample_rate(osmosdr_dev_t *dev)
{
	if (!dev)
		return 0;

	return dev->rate;
}

osmosdr_dongle_t *find_known_device(uint16_t vid, uint16_t pid)
{
	unsigned int i;
	osmosdr_dongle_t *device = NULL;

	for (i = 0; i < sizeof(known_devices)/sizeof(osmosdr_dongle_t); i++ ) {
		if (known_devices[i].vid == vid && known_devices[i].pid == pid) {
			device = &known_devices[i];
			break;
		}
	}

	return device;
}

uint32_t osmosdr_get_device_count(void)
{
	int i;
	libusb_context *ctx;
	libusb_device **list;
	struct libusb_device_descriptor dd;
	uint32_t device_count = 0;
	ssize_t cnt;

	libusb_init(&ctx);

	cnt = libusb_get_device_list(ctx, &list);

	for (i = 0; i < cnt; i++) {
		libusb_get_device_descriptor(list[i], &dd);

		if (find_known_device(dd.idVendor, dd.idProduct))
			device_count++;
	}

	libusb_free_device_list(list, 1);

	libusb_exit(ctx);

	return device_count;
}

const char *osmosdr_get_device_name(uint32_t index)
{
	int i;
	libusb_context *ctx;
	libusb_device **list;
	struct libusb_device_descriptor dd;
	osmosdr_dongle_t *device = NULL;
	uint32_t device_count = 0;
	ssize_t cnt;

	libusb_init(&ctx);

	cnt = libusb_get_device_list(ctx, &list);

	for (i = 0; i < cnt; i++) {
		libusb_get_device_descriptor(list[i], &dd);

		device = find_known_device(dd.idVendor, dd.idProduct);

		if (device) {
			device_count++;

			if (index == device_count - 1)
				break;
		}
	}

	libusb_free_device_list(list, 1);

	libusb_exit(ctx);

	if (device)
		return device->name;
	else
		return "";
}

int osmosdr_get_device_usb_strings(uint32_t index, char *manufact,
				   char *product, char *serial)
{
	int r = -2;
	int i;
	libusb_context *ctx;
	libusb_device **list;
	struct libusb_device_descriptor dd;
	osmosdr_dongle_t *device = NULL;
	osmosdr_dev_t devt;
	uint32_t device_count = 0;
	ssize_t cnt;

	libusb_init(&ctx);

	cnt = libusb_get_device_list(ctx, &list);

	for (i = 0; i < cnt; i++) {
		libusb_get_device_descriptor(list[i], &dd);

		device = find_known_device(dd.idVendor, dd.idProduct);

		if (device) {
			device_count++;

			if (index == device_count - 1) {
				r = libusb_open(list[i], &devt.devh);
				if (!r) {
					r = osmosdr_get_usb_strings(&devt,
								    manufact,
								    product,
								    serial);
					libusb_close(devt.devh);
				}
				break;
			}
		}
	}

	libusb_free_device_list(list, 1);

	libusb_exit(ctx);

	return r;
}

int osmosdr_open(osmosdr_dev_t **out_dev, uint32_t index)
{
	int r;
	int i;
	libusb_device **list;
	osmosdr_dev_t *dev = NULL;
	libusb_device *device = NULL;
	uint32_t device_count = 0;
	struct libusb_device_descriptor dd;
	uint8_t reg;
	ssize_t cnt;

	dev = malloc(sizeof(osmosdr_dev_t));
	if (NULL == dev)
		return -ENOMEM;

	memset(dev, 0, sizeof(osmosdr_dev_t));

	libusb_init(&dev->ctx);

	cnt = libusb_get_device_list(dev->ctx, &list);

	for (i = 0; i < cnt; i++) {
		device = list[i];

		libusb_get_device_descriptor(list[i], &dd);

		if (find_known_device(dd.idVendor, dd.idProduct)) {
			device_count++;
		}

		if (index == device_count - 1)
			break;

		device = NULL;
	}

	if (!device) {
		r = -1;
		goto err;
	}

	r = libusb_open(device, &dev->devh);
	if (r < 0) {
		libusb_free_device_list(list, 1);
		fprintf(stderr, "usb_open error %d\n", r);
		goto err;
	}

	libusb_free_device_list(list, 1);

	r = libusb_claim_interface(dev->devh, 0);
	if (r < 0) {
		fprintf(stderr, "usb_claim_interface error %d\n", r);
		goto err;
	}

	dev->adc_clock = DEF_ADC_FREQ;

	/* TODO: osmosdr_init_baseband(dev); */

found:
	if (dev->tuner) {
		dev->tun_clock = dev->adc_clock;

		if (dev->tuner->init) {
			/* TODO: r = dev->tuner->init(dev); */
		}
	}

	*out_dev = dev;

	return 0;
err:
	if (dev) {
		if (dev->ctx)
			libusb_exit(dev->ctx);

		free(dev);
	}

	return r;
}

int osmosdr_close(osmosdr_dev_t *dev)
{
	if (!dev)
		return -1;

	/* TODO: osmosdr_deinit_baseband(dev); */

	libusb_release_interface(dev->devh, 0);
	libusb_close(dev->devh);

	libusb_exit(dev->ctx);

	free(dev);

	return 0;
}

int osmosdr_reset_buffer(osmosdr_dev_t *dev)
{
	if (!dev)
		return -1;

	/* TODO: implement */

	return 0;
}

int osmosdr_read_sync(osmosdr_dev_t *dev, void *buf, int len, int *n_read)
{
	if (!dev)
		return -1;

	return libusb_bulk_transfer(dev->devh, 0x86, buf, len, n_read, BULK_TIMEOUT);
}

static void LIBUSB_CALL _libusb_callback(struct libusb_transfer *xfer)
{
	osmosdr_dev_t *dev = (osmosdr_dev_t *)xfer->user_data;

	if (LIBUSB_TRANSFER_COMPLETED == xfer->status) {
		if (dev->cb)
			dev->cb(xfer->buffer, xfer->actual_length, dev->cb_ctx);

		libusb_submit_transfer(xfer); /* resubmit transfer */
	} else {
		/*fprintf(stderr, "transfer status: %d\n", xfer->status);*/
		osmosdr_cancel_async(dev); /* abort async loop */
	}
}

static int _osmosdr_alloc_async_buffers(osmosdr_dev_t *dev)
{
	unsigned int i;

	if (!dev)
		return -1;

	if (!dev->xfer) {
		dev->xfer = malloc(dev->xfer_buf_num *
				   sizeof(struct libusb_transfer *));

		for(i = 0; i < dev->xfer_buf_num; ++i)
			dev->xfer[i] = libusb_alloc_transfer(0);
	}

	if (!dev->xfer_buf) {
		dev->xfer_buf = malloc(dev->xfer_buf_num *
					   sizeof(unsigned char *));

		for(i = 0; i < dev->xfer_buf_num; ++i)
			dev->xfer_buf[i] = malloc(dev->xfer_buf_len);
	}

	return 0;
}

static int _osmosdr_free_async_buffers(osmosdr_dev_t *dev)
{
	unsigned int i;

	if (!dev)
		return -1;

	if (dev->xfer) {
		for(i = 0; i < dev->xfer_buf_num; ++i) {
			if (dev->xfer[i]) {
				libusb_free_transfer(dev->xfer[i]);
			}
		}

		free(dev->xfer);
		dev->xfer = NULL;
	}

	if (dev->xfer_buf) {
		for(i = 0; i < dev->xfer_buf_num; ++i) {
			if (dev->xfer_buf[i])
				free(dev->xfer_buf[i]);
		}

		free(dev->xfer_buf);
		dev->xfer_buf = NULL;
	}

	return 0;
}

int osmosdr_read_async(osmosdr_dev_t *dev, osmosdr_read_async_cb_t cb, void *ctx,
		       uint32_t buf_num, uint32_t buf_len)
{
	unsigned int i;
	int r;
	struct timeval tv = { 1, 0 };

	if (!dev)
		return -1;

	dev->cb = cb;
	dev->cb_ctx = ctx;

	if (buf_num > 0)
		dev->xfer_buf_num = buf_num;
	else
		dev->xfer_buf_num = DEFAULT_BUF_NUMBER;

	if (buf_len > 0 && buf_len % 512 == 0) /* len must be multiple of 512 */
		dev->xfer_buf_len = buf_len;
	else
		dev->xfer_buf_len = DEFAULT_BUF_LENGTH;

	_osmosdr_alloc_async_buffers(dev);

	for(i = 0; i < dev->xfer_buf_num; ++i) {
		libusb_fill_bulk_transfer(dev->xfer[i],
					  dev->devh,
					  0x86,
					  dev->xfer_buf[i],
					  dev->xfer_buf_len,
					  _libusb_callback,
					  (void *)dev,
					  BULK_TIMEOUT);

		libusb_submit_transfer(dev->xfer[i]);
	}

	dev->async_status = OSMOSDR_RUNNING;

	while (OSMOSDR_INACTIVE != dev->async_status) {
		r = libusb_handle_events_timeout(dev->ctx, &tv);
		if (r < 0) {
			/*fprintf(stderr, "handle_events returned: %d\n", r);*/
			if (r == LIBUSB_ERROR_INTERRUPTED) /* stray signal */
				continue;
			break;
		}

		if (OSMOSDR_CANCELING == dev->async_status) {
			dev->async_status = OSMOSDR_INACTIVE;

			if (!dev->xfer)
				break;

			for(i = 0; i < dev->xfer_buf_num; ++i) {
				if (!dev->xfer[i])
					continue;

				if (dev->xfer[i]->status == LIBUSB_TRANSFER_COMPLETED) {
					libusb_cancel_transfer(dev->xfer[i]);
					dev->async_status = OSMOSDR_CANCELING;
				}
			}

			if (OSMOSDR_INACTIVE == dev->async_status)
				break;
		}
	}

	_osmosdr_free_async_buffers(dev);

	return r;
}

int osmosdr_cancel_async(osmosdr_dev_t *dev)
{
	if (!dev)
		return -1;

	if (OSMOSDR_RUNNING == dev->async_status) {
		dev->async_status = OSMOSDR_CANCELING;
		return 0;
	}

	return -2;
}
