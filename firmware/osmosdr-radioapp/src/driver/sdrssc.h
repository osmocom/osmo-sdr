#ifndef INCLUDE_SDRSSC_H
#define INCLUDE_SDRSSC_H

#include "dmabuffer.h"
#include "../board.h"

#if defined(BOARD_SAMPLE_SOURCE_SSC)
void sdrssc_configure(void);
int sdrssc_dmaStart(void);
void sdrssc_dmaStop(void);
void sdrssc_returnBuffer(DMABuffer* buffer);
void sdrssc_printStats(void);
void sdrssc_fft(void);
#endif // defined(BOARD_SAMPLE_SOURCE_SSC)

#endif // INCLUDE_SDRSSC_H
