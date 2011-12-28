#ifndef __SI570_H__
#define __SI570_H__

#include <stdint.h>

#define SI570_ADDR  0x55
#define SI570_XTAL  114285000L

struct range {
	uint32_t min;
	uint32_t max;
};

struct si570_info {
	struct range freq_range[3];
	uint32_t   init_freq;
};

//device-context
struct si570_ctx {
	int init;
	int lock;
	void *i2c;
	uint8_t	slave_addr;

	uint32_t xtal;
	const struct si570_info *info;
};

//API
int si570_init(struct si570_ctx *ctx, void *i2c_dev, uint8_t i2c_addr);
int si570_read_calibration(struct si570_ctx *ctx);
int si570_reset(struct si570_ctx *ctx);
//mode of operation
int si570_get_lock(struct si570_ctx *ctx);
//frequency
int si570_set_freq(struct si570_ctx *ctx, uint32_t freq, int trim);
//dump all registers
void si570_regdump(struct si570_ctx *ctx);

#endif //__SI570_H__
