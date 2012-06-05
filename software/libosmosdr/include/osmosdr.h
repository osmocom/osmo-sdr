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

#ifndef __OSMOSDR_H
#define __OSMOSDR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <osmosdr_export.h>

typedef struct osmosdr_dev osmosdr_dev_t;

OSMOSDR_API uint32_t osmosdr_get_device_count(void);

OSMOSDR_API const char* osmosdr_get_device_name(uint32_t index);

/*!
 * Get USB device strings.
 *
 * NOTE: The string arguments must provide space for up to 256 bytes.
 *
 * \param index the device index
 * \param manufact manufacturer name, may be NULL
 * \param product product name, may be NULL
 * \param serial serial number, may be NULL
 * \return 0 on success
 */
OSMOSDR_API int osmosdr_get_device_usb_strings(uint32_t index,
					       char *manufact,
					       char *product,
					       char *serial);

OSMOSDR_API int osmosdr_open(osmosdr_dev_t **dev, uint32_t index);

OSMOSDR_API int osmosdr_close(osmosdr_dev_t *dev);

/* configuration functions */

/*!
 * Get USB device strings.
 *
 * NOTE: The string arguments must provide space for up to 256 bytes.
 *
 * \param dev the device handle given by osmosdr_open()
 * \param manufact manufacturer name, may be NULL
 * \param product product name, may be NULL
 * \param serial serial number, may be NULL
 * \return 0 on success
 */
OSMOSDR_API int osmosdr_get_usb_strings(osmosdr_dev_t *dev, char *manufact,
					char *product, char *serial);

/*!
 * Set the frequency the device is tuned to.
 *
 * \param dev the device handle given by osmosdr_open()
 * \param freq frequency in Hz the device should be tuned to
 * \return 0 on error, frequency in Hz otherwise
 */
OSMOSDR_API int osmosdr_set_center_freq(osmosdr_dev_t *dev, uint32_t freq);

/*!
 * Get the actual frequency the device is tuned to.
 *
 * \param dev the device handle given by osmosdr_open()
 * \return 0 on error, frequency in Hz otherwise
 */
OSMOSDR_API uint32_t osmosdr_get_center_freq(osmosdr_dev_t *dev);

/*!
 * Get a list of gains supported by the tuner.
 *
 * NOTE: The gains argument must be preallocated by the caller. If NULL is
 * being given instead, the number of available gain values will be returned.
 *
 * \param dev the device handle given by osmosdr_open()
 * \param gains array of gain values. In tenths of a dB, 115 means 11.5 dB.
 * \return <= 0 on error, number of available (returned) gain values otherwise
 */
OSMOSDR_API int osmosdr_get_tuner_gains(osmosdr_dev_t *dev, int *gains);

/*!
 * Set the gain for the device.
 * Manual gain mode must be enabled for this to work.
 *
 * Valid gain values (in tenths of a dB) for the E4000 tuner:
 * -10, 15, 40, 65, 90, 115, 140, 165, 190,
 * 215, 240, 290, 340, 420, 430, 450, 470, 490
 *
 * Valid gain values may be queried with \ref osmosdr_get_tuner_gains function.
 *
 * \param dev the device handle given by osmosdr_open()
 * \param gain in tenths of a dB, 115 means 11.5 dB.
 * \return 0 on success
 */
OSMOSDR_API int osmosdr_set_tuner_gain(osmosdr_dev_t *dev, int gain);

/*!
 * Get actual gain the device is configured to.
 *
 * \param dev the device handle given by osmosdr_open()
 * \return 0 on error, gain in tenths of a dB, 115 means 11.5 dB.
 */
OSMOSDR_API int osmosdr_get_tuner_gain(osmosdr_dev_t *dev);

/*!
 * Set the gain mode (automatic/manual) for the device.
 * Manual gain mode must be enabled for the gain setter function to work.
 *
 * \param dev the device handle given by osmosdr_open()
 * \param manual gain mode, 1 means manual gain mode shall be enabled.
 * \return 0 on success
 */
OSMOSDR_API int osmosdr_set_tuner_gain_mode(osmosdr_dev_t *dev, int manual);

/* set LNA gain in hdB */
OSMOSDR_API int osmosdr_set_tuner_lna_gain(osmosdr_dev_t *dev, int gain);
/* set mixer gain in hdB */
OSMOSDR_API int osmosdr_set_tuner_mixer_gain(osmosdr_dev_t *dev, int gain);
/* set mixer enhancement */
OSMOSDR_API int osmosdr_set_tuner_mixer_enh(osmosdr_dev_t *dev, int enh);
/* set IF stages gain */
OSMOSDR_API int osmosdr_set_tuner_if_gain(osmosdr_dev_t *dev, int stage, int gain);

/*!
 * Get a list of sample rates supported by the device.
 *
 * NOTE: The rates argument must be preallocated by the caller. If NULL is
 * being given instead, the number of available rate values will be returned.
 *
 * \param dev the device handle given by osmosdr_open()
 * \param rates array of rate values in Hz
 * \return <= 0 on error, number of available (returned) rate values otherwise
 */
OSMOSDR_API uint32_t osmosdr_get_sample_rates(osmosdr_dev_t *dev, uint32_t *rates);

/*!
 * Set the sample rate for the device.
 *
 * \param dev the device handle given by osmosdr_open()
 * \param rate the sample rate in Hz
 * \return 0 on success
 */
OSMOSDR_API int osmosdr_set_sample_rate(osmosdr_dev_t *dev, uint32_t rate);

/*!
 * Get the sample rate the device is configured to.
 *
 * \param dev the device handle given by osmosdr_open()
 * \return 0 on error, sample rate in Hz otherwise
 */
OSMOSDR_API uint32_t osmosdr_get_sample_rate(osmosdr_dev_t *dev);

/* this allows direct access to the FPGA register bank */
OSMOSDR_API int osmosdr_set_fpga_reg(osmosdr_dev_t *dev, uint8_t reg, uint32_t value);

/* more access to OsmoSDR functions */

/* set decimation (0 = off, 1 = 1:2, 2 = 1:4, 3 = 1:8, ... 6 = 1:64) */
OSMOSDR_API int osmosdr_set_fpga_decimation(osmosdr_dev_t *dev, int dec);

/* set i/q swap / spectrum inversion (0 = off, 1 = on) */
OSMOSDR_API int osmosdr_set_fpga_iq_swap(osmosdr_dev_t *dev, int sw);

/* configure scaling of i and q channel (scaled_i = (orig_i * igain) / 32768) */
OSMOSDR_API int osmosdr_set_fpga_iq_gain(osmosdr_dev_t *dev, uint16_t igain, uint16_t qgain);

/* configure i and q offset correction (corrected_i = orig_i + iofs */
OSMOSDR_API int osmosdr_set_fpga_iq_ofs(osmosdr_dev_t *dev, int16_t iofs, int16_t qofs);

/* streaming functions */

OSMOSDR_API int osmosdr_reset_buffer(osmosdr_dev_t *dev);

OSMOSDR_API int osmosdr_read_sync(osmosdr_dev_t *dev, void *buf, int len, int *n_read);

typedef void(*osmosdr_read_async_cb_t)(unsigned char *buf, uint32_t len, void *ctx);

/*!
 * Read samples from the device asynchronously. This function will block until
 * it is being canceled using osmosdr_cancel_async()
 *
 * \param dev the device handle given by osmosdr_open()
 * \param cb callback function to return received samples
 * \param ctx user specific context to pass via the callback function
 * \param buf_num optional buffer count, buf_num * buf_len = overall buffer size
 *		  set to 0 for default buffer count (32)
 * \param buf_len optional buffer length, must be multiple of 512,
 *		  set to 0 for default buffer length (16 * 32 * 512)
 * \return 0 on success
 */
OSMOSDR_API int osmosdr_read_async(osmosdr_dev_t *dev,
				 osmosdr_read_async_cb_t cb,
				 void *ctx,
				 uint32_t buf_num,
				 uint32_t buf_len);

/*!
 * Cancel all pending asynchronous operations on the device.
 *
 * \param dev the device handle given by osmosdr_open()
 * \return 0 on success
 */
OSMOSDR_API int osmosdr_cancel_async(osmosdr_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* __OSMOSDR_H */
