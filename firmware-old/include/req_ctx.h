#ifndef _REQ_CTX_H
#define _REQ_CTX_H

#define __ramfunc

#define RCTX_SIZE_LARGE	2048
#define RCTX_SIZE_SMALL	1024

#define MAX_HDRSIZE	sizeof(struct openpcd_hdr)
#define MAX_REQSIZE	(64-MAX_HDRSIZE)

#define req_buf_payload(x)	(x->data[x->hdr_len])
#define req_buf_hdr(x)		(x->data[0])

#include <stdint.h>
#include <dmad/dmad.h>
#include <linuxlist.h>

struct req_ctx {
	struct llist_head list;
	volatile uint32_t state;
	uint16_t size;
	uint16_t tot_len;
	DmaLinkList dma_lli;
	uint8_t *data;
};

#define RCTX_STATE_FREE			0xfe
#define RCTX_STATE_UDP_RCV_BUSY		0x01
#define RCTX_STATE_UDP_RCV_DONE		0x02
#define RCTX_STATE_MAIN_PROCESSING	0x03

#define RCTX_STATE_UDP_EP2_PENDING	0x10
#define RCTX_STATE_UDP_EP2_BUSY		0x11

#define RCTX_STATE_UDP_EP3_PENDING	0x12
#define RCTX_STATE_UDP_EP3_BUSY		0x13

#define RCTX_STATE_SSC_RX_BUSY		0x20	/* currently being processed by Rx DMA */
#define RCTX_STATE_SSC_RX_PENDING	0x21	/* waiting in queue for Rx SSC DMA */

#define RCTX_STATE_INVALID		0xff

extern struct req_ctx __ramfunc *req_ctx_find_get(int large, unsigned long old_state, unsigned long new_state);
extern struct req_ctx *req_ctx_find_busy(void);
extern void req_ctx_set_state(struct req_ctx *ctx, unsigned long new_state);
extern void req_ctx_put(struct req_ctx *ctx);
extern uint8_t req_ctx_num(struct req_ctx *ctx);

void req_ctx_enqueue(struct llist_head *list, struct req_ctx *rctx);
struct req_ctx *req_ctx_dequeue(struct llist_head *list);

void req_ctx_dump();

#endif /* _REQ_CTX_H */
