/* AT91SAM7 USB Request Context for OpenPCD / OpenPICC
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdint.h>
#include <stdlib.h>

#include <board.h>
#define __INLINE inline
#define IRQn_Type int
#include <cmsis/core_cm3.h>

#include "req_ctx.h"


#define local_irq_save(x)	do { __disable_fault_irq(); __disable_irq(); } while(0)
#define local_irq_restore(x)	do { __enable_fault_irq(); __enable_irq(); } while(0)

#define NUM_RCTX_SMALL 20
#define NUM_RCTX_LARGE 0

#define NUM_REQ_CTX	(NUM_RCTX_SMALL+NUM_RCTX_LARGE)

static uint8_t rctx_data[NUM_RCTX_SMALL][RCTX_SIZE_SMALL];
static uint8_t rctx_data_large[NUM_RCTX_LARGE][RCTX_SIZE_LARGE];

static struct req_ctx req_ctx[NUM_REQ_CTX];

struct req_ctx __ramfunc *req_ctx_find_get(int large,
				 unsigned long old_state, 
				 unsigned long new_state)
{
	unsigned long flags;
	uint8_t i;

	if (large)
		i = NUM_RCTX_SMALL;
	else
		i = 0;

	for (; i < NUM_REQ_CTX; i++) {
		local_irq_save(flags);
		if (req_ctx[i].state == old_state) {
			req_ctx[i].state = new_state;
			local_irq_restore(flags);
			return &req_ctx[i];
		}
		local_irq_restore(flags);
	}

	return NULL;
}

uint8_t req_ctx_num(struct req_ctx *ctx)
{
	return ((char *)ctx - (char *)&req_ctx[0])/sizeof(*ctx);
}

void req_ctx_set_state(struct req_ctx *ctx, unsigned long new_state)
{
	unsigned long flags;

	/* FIXME: do we need this kind of locking, we're UP! */
	local_irq_save(flags);
	ctx->state = new_state;
	local_irq_restore(flags);
}

void req_ctx_put(struct req_ctx *ctx)
{
	req_ctx_set_state(ctx, RCTX_STATE_FREE);
}

void req_ctx_init(void)
{
	int i;

	for (i = 0; i < NUM_RCTX_SMALL; i++) {
		req_ctx[i].size = RCTX_SIZE_SMALL;
		req_ctx[i].data = rctx_data[i];
		req_ctx[i].state = RCTX_STATE_FREE;
	}

	for (i = 0; i < NUM_RCTX_LARGE; i++) {
		req_ctx[NUM_RCTX_SMALL+i].size = RCTX_SIZE_LARGE;
		req_ctx[NUM_RCTX_SMALL+i].data = rctx_data_large[i];
		req_ctx[NUM_RCTX_SMALL+i].state = RCTX_STATE_FREE;
	}
}

struct req_ctx *req_ctx_dequeue(struct llist_head *list)
{
	unsigned long flags;
	struct req_ctx *rctx;

	local_irq_save(flags);
	if (llist_empty(list)) {
		local_irq_restore(flags);
		return NULL;
	}

	rctx = llist_entry(list->next, struct req_ctx, list);
	llist_del(&rctx->list);
	local_irq_restore(flags);

	return rctx;
}

void req_ctx_enqueue(struct llist_head *list, struct req_ctx *rctx)
{
	unsigned long flags;

	/* FIXME: do we need this kind of locking, we're UP! */
	local_irq_save(flags);
	llist_add_tail(&rctx->list, list);
	local_irq_restore(flags);
}

void req_ctx_dump()
{
	int i;

	local_irq_save(flags);
	printf("ctx status: ");
	for(i = 0; i < NUM_REQ_CTX; i++)
		printf(" %02x", req_ctx[i].state);
	local_irq_restore(flags);
	printf("\n\r");
}
