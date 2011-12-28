#include <time.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>

#include <si570.h>
#include <logging.h>

static void udelay(uint32_t usec)
{
	volatile uint32_t i, j;

	for (i = 0; i < usec; i++) {
		for (j = 0; j < 0xffff; j++) {
			j = 0;
		}
	}
}

static int smbus8_read_bytes(void *i2c, uint8_t addr, uint8_t reg_nr,
			    uint8_t *out, uint8_t num)
{
	unsigned char rc;

	rc = TWID_Read(i2c, addr, reg_nr, 1, out, num, NULL);
	if (rc != 0) {
		LOGP(DVCO, LOGL_ERROR, "Error %u in TWID_Read\n", rc);
		return -EIO;
	}

	return rc;
}

static int smbus8_read_byte(void *i2c, uint8_t addr, uint8_t reg_nr,
			    uint8_t val)
{
	return smbus8_read_bytes(i2c, addr, reg_nr, &val, 1);
}

static int smbus8_write_bytes(void *i2c, uint8_t addr, uint8_t reg_nr,
			      uint8_t *val, uint8_t num)
{
	unsigned char rc;

	rc = TWID_Write(i2c, addr, reg_nr, 1, val, num, NULL);
	if (rc != 0) {
		LOGP(DVCO, LOGL_ERROR, "Error %u in TWID_Write\n", rc);
		return -EIO;
	}

	return rc;
}

static int smbus8_write_byte(void *i2c, uint8_t addr, uint8_t reg_nr,
			      uint8_t val)
{
	return smbus8_write_bytes(i2c, addr, reg_nr, &val, 1);
}

static const struct si570_info si570_data[2] = {
	{.freq_range = {{.min=10000, .max=945000}, {.min=970000, .max=1134000}, {.min=1213000, .max=1417500}}, .init_freq=856000 },
	{.freq_range = {{.min=10000, .max=280000}}, .init_freq=100000 },
};


//init module
int si570_init(struct si570_ctx *ctx, void *i2c_dev, uint8_t i2c_addr)
{
	ctx->i2c = i2c_dev;
	ctx->slave_addr = i2c_addr;

	if (0 != si570_reset(ctx)) {
		printf("SI570 reset failed.\n");
		return -EIO;
	}

	if (0 != si570_read_calibration(ctx)) {
		printf("SI570 init failed.\n");
		return -EIO;
	}

	ctx->init = 1;

	return 0;
}

void si570_print_info(struct si570_ctx *ctx)
{
	printf("SI570 XTAL: %u Hz  REF: %u Hz\n", ctx->xtal >> 3, ctx->info->init_freq * 1000);
}

static int hs_to_div(int n1)
{
	switch (n1) {
		case 0: return 4;
		case 1: return 5;
		case 2: return 6;
		case 3: return 7;
		case 5: return 9;
		case 7: return 11;
	}

	return 0;
}

int si570_reset(struct si570_ctx *ctx)
{
	smbus8_write_byte(ctx->i2c, ctx->slave_addr, 135, 80);
	udelay(1000);
	smbus8_write_byte(ctx->i2c, ctx->slave_addr, 135, 01);
	udelay(1000);

	return 0;
}

int si570_read_calibration(struct si570_ctx *ctx)
{
	int n;
	int res;
	uint8_t data[6];
	uint64_t xd, xf;
	int n1, hs_div;

	if (0 != (res = smbus8_read_bytes(ctx->i2c, ctx->slave_addr, 7, data, 6))) {
		printf ("SI570 calibration read error (%i)\n", res);
		return -EINVAL;
	}

	xd   = data[1] & 0x3F;

	xd <<= 8;
	xd  |= data[2];

	xd <<= 8;
	xd  |= data[3];

	xd <<= 8;
	xd  |= data[4];

	xd <<= 8;
	xd  |= data[5];

	n1     = ((data[0] << 2) & 0x1F) | (data[1] >> 6);
	hs_div = (data[0] >> 5);

	n = 1; {
	//for (n=0; n<2; n++) {
		long xtal_hz;
		long xtal_diff;

		xf  = si570_data[n].init_freq * 1000;
		xf *= hs_to_div(hs_div) * (n1+1);
		xf<<=31;

		ctx->xtal = (xf + (xd/2)) / xd;

		ctx->info = &si570_data[n];
	}

	return -EINVAL;
}

//set frequency
int si570_set_freq(struct si570_ctx *ctx, uint32_t freq, int trim)
{
	uint32_t dco_min = 4850000ULL;
	int mul, hsv, n1;
	int freq_in_range = 0;

	ctx->lock = 0;

	for (mul = (dco_min / freq) + 1; (freq * mul) <= 5670000; mul++) {
		for (hsv=7; hsv>=0; hsv--) {
			int hsdiv = hs_to_div(hsv);

			if (!hsdiv) continue;
			if (mul%hsdiv) continue;

			n1 = mul / hsdiv;

			if (n1 > 128) continue;

			if (n1 != 1) {
				if (n1 < 1) continue;
				if (n1 % 2) continue;
			}

			goto found_solution;
		}
	}

	printf("SI570 no solution\n");

	return -EINVAL;

found_solution:

{
	uint8_t data[6];
	uint64_t xf, xd, xr;
	uint32_t n1v;

	xf = freq;
	xf*= 1000;
	xf+= trim;

	xd = hs_to_div(hsv) * n1 * xf;
	xd<<=31;

	xr = (xd + ctx->xtal/2) / ctx->xtal;

	n1v= n1 - 1;

	data[0] = (hsv << 5) | (n1v>> 2);
	data[1] = (n1v << 6) | (xr >> 32);
	data[2] = (xr  >> 24);
	data[3] = (xr  >> 16);
	data[4] = (xr  >> 8);
	data[5] = (xr  >> 0);

	smbus8_write_byte(ctx->i2c, ctx->slave_addr, 137, 0x10);

	smbus8_write_bytes(ctx->i2c,ctx->slave_addr, 7, data, 6);

	smbus8_write_byte(ctx->i2c, ctx->slave_addr, 137, 0x00);
	smbus8_write_byte(ctx->i2c, ctx->slave_addr, 135, 0x40);

	ctx->lock = 1;
}
}

int si570_get_lock(struct si570_ctx *ctx)
{
	return ctx->init && ctx->lock;
}

//dump all registers
void si570_regdump(struct si570_ctx *ctx)
{
	uint8_t data[8];

	if (!ctx->init) {
		printf("SI570 not initialized\n");
		return;
	}

	smbus8_read_bytes(ctx->i2c, ctx->slave_addr, 7, data, 6);
	smbus8_read_byte(ctx->i2c, ctx->slave_addr, 135, &data[6]);
	smbus8_read_byte(ctx->i2c, ctx->slave_addr, 137, &data[7]);

	printf("SI570 regs: %02X %02X %02X %02X %02X %02X %02X %02X\n",
		data[0], data[1], data[2], data[3],
		data[4], data[5], data[6], data[7]);
}


