#ifndef _OSDR_SSC_H
#define _OSDR_SSC_H

#include <stdint.h>

int ssc_init(void);
int ssc_active(void);

int ssc_dma_start(void);
int ssc_dma_stop(void);

#endif
