#ifndef INCLUDE_DMABUFFER_H
#define INCLUDE_DMABUFFER_H

#include "dma.h"

typedef struct DMABuffer_ {
	void* data;
	uint size;
	DMADescriptor dma;
	struct DMABuffer_* next;
	uint id;
} DMABuffer;

typedef struct {
	DMABuffer* first;
	DMABuffer* last;
} DMABufferQueue;

void dmaBuffer_init(DMABufferQueue* queue);
void dmaBuffer_initBuffer(DMABuffer* buffer, void* data, uint size, uint id);
Bool dmaBuffer_isEmpty(const DMABufferQueue* queue);
DMABuffer* dmaBuffer_enqueue(DMABufferQueue* queue, DMABuffer* buffer);
DMABuffer* dmaBuffer_dequeue(DMABufferQueue* queue);

#endif // INCLUDE_DMABUFFER_H
