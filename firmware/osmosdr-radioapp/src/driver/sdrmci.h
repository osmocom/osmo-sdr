#ifndef INCLUDE_SDRMCI_H
#define INCLUDE_SDRMCI_H

#include "dmabuffer.h"
#include "../board.h"

#if defined(BOARD_SAMPLE_SOURCE_MCI)
void sdrmci_configure(void);
int sdrmci_dmaStart(void);
void sdrmci_dmaStop(void);
void sdrmci_returnBuffer(DMABuffer* buffer);
void sdrmci_printStats(void);
#endif // defined(BOARD_SAMPLE_SOURCE_MCI)

#endif // INCLUDE_SDRMCI_H
