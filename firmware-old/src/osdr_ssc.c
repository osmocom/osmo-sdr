
/* (C) 2011-2012 by Harald Welte <laforge@gnumonks.org>
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

#include <board.h>
#include <errno.h>
#include <irq/irq.h>
#include <dbgu/dbgu.h>
#include <ssc/ssc.h>
#include <pmc/pmc.h>
#include <utility/assert.h>
#include <utility/math.h>
#include <utility/trace.h>
#include <utility/led.h>

//#include <dmad/dmad.h>
#include <dma/dma.h>

#include <common.h>
#include <req_ctx.h>
#include <uart_cmd.h>

struct reg {
	unsigned int offset;
	const char *name;
};

void reg_dump(struct reg *regs, uint32_t *base)
{
	struct reg *r;
	for (r = regs; r->offset || r->name; r++) {
		uint32_t *addr = (uint32_t *) ((uint8_t *)base + r->offset);
		printf("%s\t%08x:\t%08x\n\r", r->name, addr, *addr);
	}
}


#define DMA_CTRLA	(AT91C_HDMA_SRC_WIDTH_WORD|AT91C_HDMA_DST_WIDTH_WORD|AT91C_HDMA_SCSIZE_1|AT91C_HDMA_DCSIZE_4)
#define DMA_CTRLB 	(AT91C_HDMA_DST_DSCR_FETCH_FROM_MEM |	\
			 AT91C_HDMA_DST_ADDRESS_MODE_INCR |	\
			 AT91C_HDMA_SRC_DSCR_FETCH_DISABLE |	\
			 AT91C_HDMA_SRC_ADDRESS_MODE_FIXED |	\
			 AT91C_HDMA_FC_PER2MEM)

struct reg dma_regs[] = {
	{ 0,	"GCFG" },
	{ 4, 	"EN" },
	{ 8,	"SREQ" },
	{ 0xC,	"CREQ" },
	{ 0x10,	"LAST" },
	{ 0x20, "EBCIMR" },
	{ 0x24, "EBCISR" },
	{ 0x30, "CHSR" },
	{ 0,	NULL}
};

struct reg dma_ch_regs[] = {
	{ 0,	"SADDR" },
	{ 4,	"DADDR" },
	{ 8,	"DSCR" },
	{ 0xC,	"CTRLA" },
	{ 0x10, "CTRLB" },
	{ 0x14, "CFG" },
	{ 0,	NULL}
};

static void dma_dump_regs(void)
{
	reg_dump(dma_regs, (uint32_t *) AT91C_BASE_HDMA);
	reg_dump(dma_ch_regs, (uint8_t *)AT91C_BASE_HDMA_CH_0 + (BOARD_SSC_DMA_CHANNEL*0x28));
}

struct ssc_state {
	struct llist_head pending_rctx;
	int hdma_chain_len;
	int active;
	uint32_t total_xfers;
	uint32_t total_irqs;
	uint32_t total_ovrun;
};

struct ssc_state ssc_state;

#define INTENDED_HDMA_C_LEN	10

static void __refill_dma()
{
	int missing = INTENDED_HDMA_C_LEN - ssc_state.hdma_chain_len;
	int i;

	for (i = 0; i < missing; i++) {
		struct req_ctx *rctx;

		/* obtain an unused request context from pool */
		rctx = req_ctx_find_get(0, RCTX_STATE_FREE, RCTX_STATE_SSC_RX_PENDING);
		if (!rctx) {
			break;
		}

		/* populate DMA descriptor inside request context */
		rctx->dma_lli.sourceAddress = (unsigned int) &AT91C_BASE_SSC0->SSC_RHR;
		rctx->dma_lli.destAddress = rctx->data;
		rctx->dma_lli.controlA = DMA_CTRLA | (rctx->size/4);
		rctx->dma_lli.controlB = DMA_CTRLB;
		rctx->dma_lli.descriptor = 0; /* end of list */

		/* append to list and update end pointer */
		if (!llist_empty(&ssc_state.pending_rctx)) {
			struct req_ctx *prev_rctx = llist_entry(ssc_state.pending_rctx.prev,
								struct req_ctx, list);
			prev_rctx->dma_lli.descriptor = &rctx->dma_lli;
		}
		req_ctx_enqueue(&ssc_state.pending_rctx, rctx);
		ssc_state.hdma_chain_len++;
	}
	/*
	if (ssc_state.hdma_chain_len <= 1)
		TRACE_ERROR("Unable to get rctx for SSC DMA refill\n\r");
	*/
}

int ssc_dma_start(void)
{
	struct req_ctx *rctx;

	__refill_dma();

	if (ssc_state.active) {
		//TRACE_WARNING("Cannot start SSC DMA, active == 1\n\r");
		return -EBUSY;
	}

	if (llist_empty(&ssc_state.pending_rctx)) {
		//TRACE_WARNING("Cannot start SSC DMA, no rctx pending\n\r");
		return -ENOMEM;
	}

	rctx = llist_entry(ssc_state.pending_rctx.next, struct req_ctx, list);

	/* clear any pending interrupts */
	DMA_DisableChannel(BOARD_SSC_DMA_CHANNEL);
	DMA_GetStatus();

	DMA_SetDescriptorAddr(BOARD_SSC_DMA_CHANNEL, &rctx->dma_lli);
	DMA_SetSourceAddr(BOARD_SSC_DMA_CHANNEL, &AT91C_BASE_SSC0->SSC_RHR);
	DMA_SetSourceBufferMode(BOARD_SSC_DMA_CHANNEL, DMA_TRANSFER_LLI,
				(AT91C_HDMA_SRC_ADDRESS_MODE_FIXED >> 24));
	DMA_SetDestBufferMode(BOARD_SSC_DMA_CHANNEL, DMA_TRANSFER_LLI,
				(AT91C_HDMA_DST_ADDRESS_MODE_INCR >> 28));
	DMA_SetFlowControl(BOARD_SSC_DMA_CHANNEL, AT91C_HDMA_FC_PER2MEM >> 21);
	DMA_SetConfiguration(BOARD_SSC_DMA_CHANNEL,
				BOARD_SSC_DMA_HW_SRC_REQ_ID | BOARD_SSC_DMA_HW_DEST_REQ_ID
				| AT91C_HDMA_SRC_H2SEL_HW \
				| AT91C_HDMA_DST_H2SEL_SW \
				| AT91C_HDMA_SOD_DISABLE \
				| AT91C_HDMA_FIFOCFG_LARGESTBURST);

	ssc_state.active = 1;
	DMA_EnableChannel(BOARD_SSC_DMA_CHANNEL);
	LED_Set(0);
	TRACE_INFO("Started SSC DMA\n\r");

	SSC_EnableReceiver(AT91C_BASE_SSC0);

	return 0;
}

int ssc_dma_stop(void)
{
	SSC_DisableReceiver(AT91C_BASE_SSC0);
	ssc_state.active = 0;

	/* clear any pending interrupts */
	DMA_DisableChannel(BOARD_SSC_DMA_CHANNEL);
	DMA_GetStatus();

	return 0;
}


#define BTC(N)	(1 << N)
#define CBTC(N)	(1 << (8+N))
#define	ERR(N)	(1 << (16+N))


/* for some strange reason this cannot be static! */
void HDMA_IrqHandler(void)
{
	unsigned int status = DMA_GetStatus();
	struct req_ctx *rctx, *rctx2;

	ssc_state.total_irqs++;

	if (status & BTC(BOARD_SSC_DMA_CHANNEL)) {
		llist_for_each_entry_safe(rctx, rctx2, &ssc_state.pending_rctx, list) {
			if (!(rctx->dma_lli.controlA & AT91C_HDMA_DONE))
				continue;

			/* a single buffer has been completed */
			ssc_state.total_xfers++;
			llist_del(&rctx->list);
			ssc_state.hdma_chain_len--;
			rctx->tot_len = rctx->size;
#if 1
			usb_submit_req_ctx(rctx);
#else
			req_ctx_set_state(rctx, RCTX_STATE_FREE);
#endif

			__refill_dma();

		}
	}
	if (status & CBTC(BOARD_SSC_DMA_CHANNEL)) {
		/* the end of the list was reached */
		LED_Clear(0);
		SSC_DisableReceiver(AT91C_BASE_SSC0);
		ssc_state.active = 0;
		TRACE_WARNING("SSC DMA buffer end reached, disabling after %u/%u\n\r",
				ssc_state.total_irqs, ssc_state.total_xfers);
	}
}

void ssc_stats(void)
{
	printf("SSC num_irq=%u, num_xfers=%u, num_ovrun=%u\n\r",
		ssc_state.total_irqs, ssc_state.total_xfers,
		ssc_state.total_ovrun);
}

void SSC0_IrqHandler(void)
{
	if (AT91C_BASE_SSC0->SSC_SR & AT91C_SSC_OVRUN)
		ssc_state.total_ovrun++;
}

int ssc_active(void)
{
	return ssc_state.active;
}


static int cmd_ssc_start(struct cmd_state *cs, enum cmd_op op,
			 const char *cmd, int argc, char **argv)
{
	ssc_init();
	ssc_dma_start();
	return 0;
}
static int cmd_ssc_stop(struct cmd_state *cs, enum cmd_op op,
			 const char *cmd, int argc, char **argv)
{
	ssc_dma_stop();
	return 0;
}
static int cmd_ssc_stats(struct cmd_state *cs, enum cmd_op op,
			 const char *cmd, int argc, char **argv)
{
	ssc_stats();
	return 0;
}
static int cmd_ssc_dump(struct cmd_state *cs, enum cmd_op op,
			const char *cmd, int argc, char **argv)
{
	struct req_ctx *rctx, *rctx2;

	dma_dump_regs();
	req_ctx_dump();

	printf("ssc pending:");
	llist_for_each_entry_safe(rctx, rctx2, &ssc_state.pending_rctx, list)
		printf(" %02d", req_ctx_num(rctx));
	printf("\n\r");
	fastsource_dump();
}

static struct cmd cmds[] = {
	{ "ssc.start", CMD_OP_EXEC, cmd_ssc_start,
	  "Start the SSC Receiver" },
	{ "ssc.stop", CMD_OP_EXEC, cmd_ssc_stop,
	  "Start the SSC Receiver" },
	{ "ssc.stats", CMD_OP_EXEC, cmd_ssc_stats,
	  "Statistics about the SSC" },
	{ "ssc.dump", CMD_OP_EXEC, cmd_ssc_dump,
	  "Dump SSC DMA registers" },
};

int ssc_init(void)
{
	memset(&ssc_state, 0, sizeof(ssc_state));
	INIT_LLIST_HEAD(&ssc_state.pending_rctx);

	SSC_DisableReceiver(AT91C_BASE_SSC0);
	SSC_Configure(AT91C_BASE_SSC0, AT91C_ID_SSC0, 0, BOARD_MCK);
	SSC_ConfigureReceiver(AT91C_BASE_SSC0, AT91C_SSC_CKS_RK | AT91C_SSC_CKO_NONE |
					 AT91C_SSC_CKG_NONE | AT91C_SSC_START_RISE_RF |
					 AT91C_SSC_CKI,
					 AT91C_SSC_MSBF | (32-1) );

	SSC_DisableInterrupts(AT91C_BASE_SSC0, 0xffffffff);
	SSC_EnableInterrupts(AT91C_BASE_SSC0, AT91C_SSC_OVRUN);

	/* Enable Overrun interrupts */
	IRQ_ConfigureIT(AT91C_ID_SSC0, 0, SSC0_IrqHandler);
	IRQ_EnableIT(AT91C_ID_SSC0);

	/* Enable DMA controller and register interrupt handler */
	PMC_EnablePeripheral(AT91C_ID_HDMA);
	DMA_Enable();
	IRQ_ConfigureIT(AT91C_ID_HDMA, 0, HDMA_IrqHandler);
	IRQ_EnableIT(AT91C_ID_HDMA);
	DMA_EnableIt(BTC(BOARD_SSC_DMA_CHANNEL) | CBTC(BOARD_SSC_DMA_CHANNEL) |
		     ERR(BOARD_SSC_DMA_CHANNEL));

	TRACE_INFO("SSC initialized\n\r");
	LED_Clear(0);

	uart_cmds_register(cmds, ARRAY_SIZE(cmds));

	return 0;
}
