#ifndef INCLUDE_SSC_H
#define INCLUDE_SSC_H

#include "../at91sam3u4/AT91SAM3U4.h"
#include "../crt/types.h"

void ssc_configure(AT91S_SSC* ssc, uint id, uint bitRate);
void ssc_configureReceiver(AT91S_SSC *ssc, uint rcmr, uint rfmr);
void ssc_disableInterrupts(AT91S_SSC* ssc, uint sources);
void ssc_enableInterrupts(AT91S_SSC* ssc, uint sources);
void ssc_enableReceiver(AT91S_SSC* ssc);
void ssc_disableReceiver(AT91S_SSC* sc);

#endif // INCLUDE_SSC_H
