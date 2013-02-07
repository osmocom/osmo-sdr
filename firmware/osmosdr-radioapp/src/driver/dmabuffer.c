/* (C) 2011-2012 by Christian Daniel <cd@maintech.de>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include "dmabuffer.h"
#include "../board.h"
#include "../at91sam3u4/core_cm3.h"

void dmaBuffer_init(DMABufferQueue* queue)
{
	queue->first = NULL;
	queue->last = NULL;
}

void dmaBuffer_initBuffer(DMABuffer* buffer, void* data, uint size, uint id)
{
	buffer->data = data;
	buffer->size = size;
	buffer->next = NULL;
	buffer->id = id;
}

Bool dmaBuffer_isEmpty(const DMABufferQueue* queue)
{
	// no lock needed - should be atomic anyways
	return queue->first == NULL ? True : False;
}

DMABuffer* dmaBuffer_enqueue(DMABufferQueue* queue, DMABuffer* buffer)
{
	u32 faultmask = __get_FAULTMASK();
	DMABuffer* prev;

	__disable_fault_irq();

	prev = queue->last;
	buffer->next = NULL;
	queue->last->next = buffer;
	queue->last = buffer;
	if(queue->first == NULL)
		queue->first = buffer;

	__set_FAULTMASK(faultmask);

	return prev;
}

DMABuffer* dmaBuffer_dequeue(DMABufferQueue* queue)
{
	u32 faultmask = __get_FAULTMASK();
	DMABuffer* buffer;

	__disable_fault_irq();

	buffer = queue->first;
	if(buffer != NULL) {
		queue->first = buffer->next;
		if(queue->first == NULL)
			queue->last = NULL;
	}

	__set_FAULTMASK(faultmask);

	return buffer;
}
