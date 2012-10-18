
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

#include <common.h>
#include <tuner_e4k.h>


void logp2(int subsys, unsigned int level, char *file,
	   int line, int cont, const char *format, ...)
{
	va_list ap;
	fprintf(stderr, "%u/%u/%s:%u: ", subsys, level, file, line);

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}


static uint8_t regs[0x7f];

/* stub functions for register read/write */
int e4k_reg_write(struct e4k_state *e4k, uint8_t reg, uint8_t val)
{
	printf("REG WRITE: [0x%02x] = 0x%02x\n", reg, val);

	if (reg > ARRAY_SIZE(regs))
		return -ERANGE;

	regs[reg] = val;

	return 0;
}

int e4k_reg_read(struct e4k_state *e4k, uint8_t reg)
{
	if (reg > ARRAY_SIZE(regs))
		return -ERANGE;

	printf("REG READ:  [0x%02x] = 0x%02x\n", reg, regs[reg]);

	return regs[reg];
}

static struct e4k_state g_e4k;

#define FOSC	26000000

static void dump_params(struct e4k_pll_params *p)
{
	int32_t delta = p->intended_flo - p->flo;

	printf("Flo_int = %u: R=%u, X=%u, Z=%u, Flo = %u, delta=%d\n",
		p->intended_flo, p->r, p->x, p->z, p->flo, delta);
}

static void compute_and_dump(uint32_t flo)
{
	struct e4k_pll_params params;
	int rc;

	memset(&params, 0, sizeof(params));
	rc = e4k_compute_pll_params(&params, FOSC, flo);
	if (rc < 0) {
		fprintf(stderr, "something went wrong!\n");
		exit(1);
	}

	dump_params(&params);
	e4k_tune_params(&g_e4k, &params);
}

static const uint32_t test_freqs[] = {
	888000000, 66666666, 425000000
};
	//1234567890

int main(int argc, char **argv)
{
	int i;

	printf("Initializing....\n");
	e4k_init(&g_e4k);

	for (i = 0; i < ARRAY_SIZE(test_freqs); i++) {
		compute_and_dump(test_freqs[i]);
	}
}
