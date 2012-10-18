#ifndef INCLUDE_DMA_H
#define INCLUDE_DMA_H

#include "../at91sam3u4/AT91SAM3U4.h"
#include "../crt/types.h"

#define DMA_TRANSFER_SINGLE      0
#define DMA_TRANSFER_LLI         1
#define DMA_TRANSFER_RELOAD      2
#define DMA_TRANSFER_CONTIGUOUS  3

typedef struct {
	/// Source address.
	u32 sourceAddress;
	/// Destination address.
	u32 destAddress;
	/// Control A value.
	u32 controlA;
	/// Control B value.
	u32 controlB;
	/// Next Descriptor Address.
	u32 nextDescriptor;
} DMADescriptor;

void dma_enable(void);
void dma_enableIrq(uint flag);
void dma_enableChannel(uint channel);
void dma_disableChannel(uint channel);
uint dma_getStatus(void);
void dma_setDescriptorAddr(uint channel, uint address);
void dma_setSourceAddr(uint channel, uint address);
void dma_setSourceBufferMode(uint channel, uint transferMode, uint addressingType);
void dma_setDestBufferMode(uint channel, uint transferMode, uint addressingType);
void dma_setFlowControl(uint channel, uint flow);
void dma_setConfiguration(uint channel, uint value);

#endif // INCLUDE_DMA_H
