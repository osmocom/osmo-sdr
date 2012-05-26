
/* (C) 2011-2012 by Harald Welte <laforge@gnumonks.org>
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

#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <common.h>
#include <logging.h>
#include <tuner_e4k.h>

//#include <twi/twi.h>
#include <twi/twid.h>
#include <pio/pio.h>
#include <board.h>


int e4k_reg_write(struct e4k_state *e4k, uint8_t reg, uint8_t val)
{
	unsigned char rc;

	rc = TWID_Write(e4k->i2c_dev, e4k->i2c_addr, reg, 1, &val, 1, NULL);
	if (rc != 0) {
		LOGP(DTUN, LOGL_ERROR, "Error %u in TWID_Write\n", rc);
		return -EIO;
	}

	return 0;
}

uint8_t e4k_reg_read(struct e4k_state *e4k, uint8_t reg)
{
	unsigned char rc;
	uint8_t val;

	rc = TWID_Read(e4k->i2c_dev, e4k->i2c_addr, reg, 1, &val, 1, NULL);
	if (rc != 0) {
		LOGP(DTUN, LOGL_ERROR, "Error %u in TWID_Read\n", rc);
		return -EIO;
	}

	return val;
}


/* We assume the caller has already done TWID_initialize() */
int sam3u_e4k_init(struct e4k_state *e4k, void *i2c, uint8_t slave_addr)
{
	e4k->i2c_dev = i2c;
	e4k->i2c_addr = slave_addr;

	return 0;
}

static const Pin pin_rfstby = PIN_RFSTBY;
static const Pin pin_pwdn = PIN_PDWN;

/*! \brief Enable or disable Power of E4K
 *  \param[in] e4k E4K reference
 *  \param[in] on Enable (1) or disable (0) Power
 */
void sam3u_e4k_power(struct e4k_state *e4k, int on)
{
	if (on)
		PIO_Set(&pin_pwdn);
	else
		PIO_Clear(&pin_pwdn);
}

/*! \brief Enable or disable standby mode of E4K
 *  \param[in] e4k E4K reference
 *  \param[in] on Enable (1) or disable (0) STBY
 */
void sam3u_e4k_stby(struct e4k_state *e4k, int on)
{
	if (on)
		PIO_Clear(&pin_rfstby);
	else
		PIO_Set(&pin_rfstby);
}
