#ifndef INCLUDE_MCI_H
#define INCLUDE_MCI_H

#include "../at91sam3u4/AT91SAM3U4.h"
#include "../crt/types.h"

void mci_configure(AT91S_MCI* mci, uint id);
void mci_startStream(AT91S_MCI* mci);
void mci_stopStream(AT91S_MCI* mci);
void mci_enableInterrupts(AT91S_MCI* mci, u32 irqs);
void mci_disableInterrupts(AT91S_MCI* mci, u32 irqs);

#endif // INCLUDE_MCI_H
