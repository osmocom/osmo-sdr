
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <common.h>
#include <logging.h>
#include <tuner_e4k.h>

//#include <twi/twi.h>
#include <twi/twid.h>


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

int e4k_reg_read(struct e4k_state *e4k, uint8_t reg)
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
int sam3u_e4k_init(struct e4k_state *e4k, uint8_t slave_addr)
{
	e4k->i2c_dev = AT91C_BASE_TWI0;
	e4k->i2c_addr = slave_addr;

	return 0;
}
