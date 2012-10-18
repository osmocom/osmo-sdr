
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

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <board.h>
#include <utility/trace.h>
#include <utility/led.h>

#include <usb/common/core/USBGenericRequest.h>
#include <usb/device/core/USBD.h>
#include <usb/device/core/USBDDriver.h>
#include <usb/device/core/USBDDriverDescriptors.h>
#include <usb/device/core/USBDCallbacks.h>
#include <usb/common/audio/AUDGenericRequest.h>
#include <usb/common/audio/AUDFeatureUnitRequest.h>
#include <usb/common/audio/AUDFeatureUnitDescriptor.h>
#include <common.h>

#include <fast_source_descr.h>
#include <fast_source.h>

#include <tuner_e4k.h>
#include <si570.h>
#include <osdr_fpga.h>

#define OSMOSDR_CTRL_WRITE 0x07
#define OSMOSDR_CTRL_READ 0x87

extern const USBDDriverDescriptors auddFastSourceDriverDescriptors;
unsigned char fastsource_interfaces[3];
static USBDDriver fast_source_driver;

struct rctx_stats {
	uint32_t total;		/* total number of samples */
	uint32_t sum_ffff;	/* samples with I+Q = 0xffff */
	uint32_t sum_fffe;	/* samples with I+Q = 0xfffe */
	uint32_t sum_other;	/* samples with I+Q = something else */

	uint32_t delta_1;	/* delta of last I to current I == 1 */
	uint32_t delta_2;
	uint32_t delta_3;
	uint32_t delta_other;
};

struct usb_state {
	struct llist_head queue;
	int active;
	uint8_t muted;
#ifdef FPGA_TEST_STATS
	struct rctx_stats stats;
#endif
};
static struct usb_state usb_state;

#define EP_NR 6

static void fastsource_get_feat_cur_val(uint8_t entity, uint8_t channel,
				   uint8_t control, uint8_t length)
{
	TRACE_INFO("get_feat(E%u, CN%u, CS%u, L%u) ", entity, channel, control, length);
	if (channel == 0 && control == AUDFeatureUnitDescriptor_MUTE && length == 1)
		USBD_Write(0, &usb_state.muted, sizeof(usb_state.muted), 0, 0);
	else
		USBD_Stall(0);
}

static void fastsource_set_feat_cur_val(uint8_t entity, uint8_t channel,
				   uint8_t control, uint8_t length)
{
	TRACE_INFO("set_feat(E%u, CO%u, CH%u, L%u) ", entity, channel, control, length);
	if (channel == 0 && control == AUDFeatureUnitDescriptor_MUTE && length == 1)
		USBD_Read(0, &usb_state.muted, sizeof(usb_state.muted), 0, 0);
	else
		USBD_Stall(0);
}

static void handle_osmosdr_read(const USBGenericRequest* request)
{
	int len = USBGenericRequest_GetLength(request);
	printf("OsmoSDR GET request: type:%d, request:%d, value:%d, index: %d, length: %d\n\r",
		USBGenericRequest_GetType(request),
		USBGenericRequest_GetRequest(request),
		USBGenericRequest_GetValue(request),
		USBGenericRequest_GetIndex(request),
		len);
	USBD_Stall(0);
}

static uint32_t read_bytewise32(const uint8_t* data)
{
	return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static uint16_t read_bytewise16(const uint8_t* data)
{
	return (data[0] << 8) | data[1];
}

typedef struct Request_ {
	uint16_t func;
	uint16_t len;
} Request;

#define FUNC(group, function) ((group << 8) | function)

#define GROUP_GENERAL 0x00
#define GROUP_FPGA_V2 0x01
#define GROUP_VCXO_SI570 0x02
#define GROUP_TUNER_E4K 0x03

const static Request g_writeRequests[] = {
	// general api
	{ FUNC(GROUP_GENERAL, 0x00), 0 }, // init whatever
	{ FUNC(GROUP_GENERAL, 0x01), 0 }, // power down
	{ FUNC(GROUP_GENERAL, 0x02), 0 }, // power up

	// fpga commands
	{ FUNC(GROUP_FPGA_V2, 0x00), 0 }, // fpga init
	{ FUNC(GROUP_FPGA_V2, 0x01), 5 }, // osdr_fpga_reg_write(uint8_t reg, uint32_t val)
	{ FUNC(GROUP_FPGA_V2, 0x02), 1 }, // osdr_fpga_set_decimation(uint8_t val)
	{ FUNC(GROUP_FPGA_V2, 0x03), 1 }, // osdr_fpga_set_iq_swap(uint8_t val)
	{ FUNC(GROUP_FPGA_V2, 0x04), 4 }, // osdr_fpga_set_iq_gain(uint16_t igain, uint16_t qgain)
	{ FUNC(GROUP_FPGA_V2, 0x05), 4 }, // osdr_fpga_set_iq_ofs(int16_t iofs, int16_t qofs)

	// si570 vcxo commads
	{ FUNC(GROUP_VCXO_SI570, 0x00), 0 }, // si570_init()
	{ FUNC(GROUP_VCXO_SI570, 0x01), 16 }, // si570_reg_write
	{ FUNC(GROUP_VCXO_SI570, 0x02), 8 }, // si570_set_freq(uint32_t freq, int trim);

	// e4000 tuner commands
	{ FUNC(GROUP_TUNER_E4K, 0x00), 0 }, // e4k_init()
	{ FUNC(GROUP_TUNER_E4K, 0x01), 0 }, // reg write
	{ FUNC(GROUP_TUNER_E4K, 0x02), 5 }, // e4k_if_gain_set(uint8_t stage, int8_t value)
	{ FUNC(GROUP_TUNER_E4K, 0x03), 1 }, // e4k_mixer_gain_set(struct e4k_state *e4k, int8_t value)
	{ FUNC(GROUP_TUNER_E4K, 0x04), 1 }, // e4k_commonmode_set(int8_t value)
	{ FUNC(GROUP_TUNER_E4K, 0x05), 4 }, // e4k_tune_freq(uint32_t freq)
	{ FUNC(GROUP_TUNER_E4K, 0x06), 5 }, // e4k_if_filter_bw_set(enum e4k_if_filter filter, uint32_t bandwidth)
	{ FUNC(GROUP_TUNER_E4K, 0x07), 1 }, // e4k_if_filter_chan_enable(int on)
	{ FUNC(GROUP_TUNER_E4K, 0x08), 4 }, // e4k_manual_dc_offset(int8_t iofs, int8_t irange, int8_t qofs, int8_t qrange)
	{ FUNC(GROUP_TUNER_E4K, 0x09), 0 }, // e4k_dc_offset_calibrate()
	{ FUNC(GROUP_TUNER_E4K, 0x0a), 0 }, // e4k_dc_offset_gen_table()
	{ FUNC(GROUP_TUNER_E4K, 0x0b), 4 }, // e4k_set_lna_gain(int32_t gain)
	{ FUNC(GROUP_TUNER_E4K, 0x0c), 1 }, // e4k_enable_manual_gain(uint8_t manual)
	{ FUNC(GROUP_TUNER_E4K, 0x0d), 4 }, // e4k_set_enh_gain(int32_t gain)
};

typedef struct WriteState_ {
	uint8_t data[16];
	uint16_t func;
} WriteState;

static WriteState g_writeState;
extern struct e4k_state e4k;
extern struct si570_ctx si570;

static void finalize_write(void *pArg, unsigned char status, unsigned int transferred, unsigned int remaining)
{
	int res;

	if((status != 0) ||(remaining != 0)) {
		USBD_Stall(0);
		return;
	}

	printf("Func: %04x ...", g_writeState.func);

	switch(g_writeState.func) {
		// general api
		case FUNC(GROUP_GENERAL, 0x00): // init all
			printf("general_init()");
			res = 0; // no op so far
			break;
		case FUNC(GROUP_GENERAL, 0x01): // power down
			printf("general_power_down()");
			osdr_fpga_power(0);
			sam3u_e4k_stby(&e4k, 1);
			sam3u_e4k_power(&e4k, 0);
			res = 0;
			break;
		case FUNC(GROUP_GENERAL, 0x02): // power up
			printf("general_power_up()");
			osdr_fpga_power(1);
			sam3u_e4k_power(&e4k, 1);
			sam3u_e4k_stby(&e4k, 0);
			res = 0;
			break;

		// fpga commands
		case FUNC(GROUP_FPGA_V2, 0x00): // fpga init
			printf("fpga_v2_init()");
			res = 0; // no op so far
			break;
		case FUNC(GROUP_FPGA_V2, 0x01):
			printf("fpga_v2_reg_write()");
			osdr_fpga_reg_write(g_writeState.data[0], read_bytewise32(g_writeState.data + 1));
			res = 0;
			break;
		case FUNC(GROUP_FPGA_V2, 0x02):
			printf("osdr_fpga_set_decimation()");
			osdr_fpga_set_decimation(g_writeState.data[0]);
			res = 0;
			break;
		case FUNC(GROUP_FPGA_V2, 0x03):
			printf("osdr_fpga_set_iq_swap()");
			osdr_fpga_set_iq_swap(g_writeState.data[0]);
			res = 0;
			break;
		case FUNC(GROUP_FPGA_V2, 0x04):
			printf("osdr_fpga_set_iq_gain()");
			osdr_fpga_set_iq_gain(read_bytewise16(g_writeState.data), read_bytewise16(g_writeState.data + 2));
			res = 0;
			break;
		case FUNC(GROUP_FPGA_V2, 0x05):
			printf("osdr_fpga_set_iq_ofs()");
			osdr_fpga_set_iq_ofs(read_bytewise16(g_writeState.data), read_bytewise16(g_writeState.data + 2));
			res = 0;
			break;

		// si570 vcxo commands
		case FUNC(GROUP_VCXO_SI570, 0x00): // si570_init()
			printf("si570_init()");
			res = si570_reinit(&si570);
			break;
		case FUNC(GROUP_VCXO_SI570, 0x01):
			printf("si570_reg_write()");
			res = si570_reg_write(&si570, g_writeState.data[0], g_writeState.data[1], g_writeState.data + 2);
			break;
		case FUNC(GROUP_VCXO_SI570, 0x02):
			printf("si570_set_freq()");
			res = si570_set_freq(&si570, read_bytewise32(g_writeState.data), read_bytewise32(g_writeState.data + 4));
			break;

		// e4000 tuner commands
		case FUNC(GROUP_TUNER_E4K, 0x00):
			printf("e4k_init()");
			res = e4k_init(&e4k);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x01): // reg write
			printf("e4k_reg_write()");
			res = -1;
			break;
		case FUNC(GROUP_TUNER_E4K, 0x02):
			printf("e4k_if_gain_set()");
			res = e4k_if_gain_set(&e4k, g_writeState.data[0], read_bytewise32(g_writeState.data + 1));
			break;
		case FUNC(GROUP_TUNER_E4K, 0x03):
			printf("e4k_mixer_gain_set()");
			res = e4k_mixer_gain_set(&e4k, g_writeState.data[0]);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x04):
			printf("e4K_commonmode_set()");
			res = e4k_commonmode_set(&e4k, g_writeState.data[0]);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x05):
			printf("e4k_tune_freq()");
			res = e4k_tune_freq(&e4k, read_bytewise32(g_writeState.data));
			break;
		case FUNC(GROUP_TUNER_E4K, 0x06):
			printf("e4k_if_filter_bw_set()");
			res = e4k_if_filter_bw_set(&e4k, g_writeState.data[0], read_bytewise32(g_writeState.data + 1));
			break;
		case FUNC(GROUP_TUNER_E4K, 0x07):
			printf("e4k_if_filter_chan_enable()");
			res = e4k_if_filter_chan_enable(&e4k, g_writeState.data[0]);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x08):
			printf("e4k_manual_dc_offset()");
			res = e4k_manual_dc_offset(&e4k, g_writeState.data[0], g_writeState.data[1], g_writeState.data[2], g_writeState.data[3]);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x09):
			printf("e4k_dc_offset_calibrate()");
			res = e4k_dc_offset_calibrate(&e4k);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x0a):
			printf("e4k_dc_offset_gen_table()");
			res = e4k_dc_offset_gen_table(&e4k);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x0b):
			printf("e4k_set_lna_gain()");
			res = e4k_set_lna_gain(&e4k, read_bytewise32(g_writeState.data));
			if(res == -EINVAL)
				res = -1;
			else res = 0;
			break;
		case FUNC(GROUP_TUNER_E4K, 0x0c):
			printf("e4k_enable_manual_gain()");
			res = e4k_enable_manual_gain(&e4k, g_writeState.data[0]);
			break;
		case FUNC(GROUP_TUNER_E4K, 0x0d):
			printf("e4k_set_enh_gain()");
			res = e4k_set_enh_gain(&e4k, read_bytewise32(g_writeState.data));
			break;

		default:
			res = -1;
			break;
	}

	printf(" res: %d\n\r", res);

	if(res == 0)
		USBD_Write(0, 0, 0, 0, 0);
	else USBD_Stall(0);
}

static void handle_osmosdr_write(const USBGenericRequest* request)
{
	uint16_t func = USBGenericRequest_GetValue(request);
	int len = USBGenericRequest_GetLength(request);
	int i;
/*
	printf("OsmoSDR SET request: type:%d, request:%d, value:%04x, index: %04x, length: %d\n\r",
		USBGenericRequest_GetType(request),
		USBGenericRequest_GetRequest(request),
		USBGenericRequest_GetValue(request),
		USBGenericRequest_GetIndex(request),
		len);
*/
	for(i = 0; i < ARRAY_SIZE(g_writeRequests); i++) {
		if(g_writeRequests[i].func == func)
			break;
	}
	if(i == ARRAY_SIZE(g_writeRequests)) {
		USBD_Stall(0);
		return;
	}
	if(len != g_writeRequests[i].len) {
		USBD_Stall(0);
		return;
	}

	g_writeState.func = func;

	if(len > 0)
		USBD_Read(0, g_writeState.data, len, finalize_write, 0);
	else finalize_write(NULL, 0, 0, 0);
}

/* handler for EP0 (control) requests */
void fastsource_req_hdlr(const USBGenericRequest *request)
{
	unsigned char entity;
	unsigned char interface;

	switch (USBGenericRequest_GetType(request)) {
	case USBGenericRequest_STANDARD:
		USBDDriver_RequestHandler(&fast_source_driver, request);
		return;
	case USBGenericRequest_CLASS:
		/* continue below */
		break;
	case USBGenericRequest_VENDOR:
		if(USBGenericRequest_GetRequest(request) == OSMOSDR_CTRL_WRITE)
			handle_osmosdr_write(request);
		else if(USBGenericRequest_GetRequest(request) == OSMOSDR_CTRL_READ)
			handle_osmosdr_read(request);
		else USBD_Stall(0);
		return;
	default:
		TRACE_WARNING("Unsupported request type %u\n\r",
				USBGenericRequest_GetType(request));
		USBD_Stall(0);
		return;
	}

	switch (USBGenericRequest_GetRequest(request)) {
	case AUDGenericRequest_SETCUR:
		entity = AUDGenericRequest_GetEntity(request);
		interface = AUDGenericRequest_GetInterface(request);
		if (((entity == AUDDLoopRecDriverDescriptors_FEATUREUNIT) ||
		     (entity == AUDDLoopRecDriverDescriptors_FEATUREUNIT_REC)) &&
		    (interface == AUDDLoopRecDriverDescriptors_CONTROL)) {
			fastsource_set_feat_cur_val(entity,
				AUDFeatureUnitRequest_GetChannel(request),
				AUDFeatureUnitRequest_GetControl(request),
				USBGenericRequest_GetLength(request));
		} else {
			TRACE_WARNING("Unsupported entity/interface combination 0x%04x\n\r",
					USBGenericRequest_GetIndex(request));
			USBD_Stall(0);
		}
		break;
	case AUDGenericRequest_GETCUR:
		entity = AUDGenericRequest_GetEntity(request);
		interface = AUDGenericRequest_GetInterface(request);
		if (((entity == AUDDLoopRecDriverDescriptors_FEATUREUNIT) ||
		     (entity == AUDDLoopRecDriverDescriptors_FEATUREUNIT_REC)) &&
		    (interface == AUDDLoopRecDriverDescriptors_CONTROL)) {
			fastsource_get_feat_cur_val(entity,
				AUDFeatureUnitRequest_GetChannel(request),
				AUDFeatureUnitRequest_GetControl(request),
				USBGenericRequest_GetLength(request));
		} else {
			TRACE_WARNING("Unsupported entity/interface combination 0x%04x\n\r",
					USBGenericRequest_GetIndex(request));
			USBD_Stall(0);
		}
		break;

	default:
		TRACE_WARNING("Unsupported request %u\n\r",
				USBGenericRequest_GetIndex(request));
		USBD_Stall(0);
		break;
	}
}

/* Initialize the driver */
void fastsource_init(void)
{
	memset(&usb_state, 0, sizeof(usb_state));
	memset(fastsource_interfaces, 0x00, sizeof(fastsource_interfaces));

	INIT_LLIST_HEAD(&usb_state.queue);

	USBDDriver_Initialize(&fast_source_driver, &auddFastSourceDriverDescriptors,
				fastsource_interfaces);

	USBD_Init();
}

static int refill_dma(void);

/* completion callback: USBD_Write() has completed an IN transfer */
static void wr_compl_cb(void *arg, unsigned char status, unsigned int transferred,
			unsigned int remain)
{
	struct req_ctx *rctx = arg;

	usb_state.active = 0;

	req_ctx_set_state(rctx, RCTX_STATE_FREE);

	if (status == 0 && remain == 0) {
		refill_dma();
	} else {
		TRACE_WARNING("Err: EP%u wr_compl, status 0x%02u, xfr %u, remain %u\r\n",
				EP_NR, status, transferred, remain);
	}
}

static int refill_dma(void)
{
	struct req_ctx *rctx;
	int res;

	rctx = req_ctx_dequeue(&usb_state.queue);
	if (!rctx) {
		//TRACE_WARNING("No rctx for re-filling USB DMA\n\r");
		usb_state.active = 0;
		return -ENOENT;
	}

	req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_BUSY);

	if ((res = USBD_Write(EP_NR, rctx->data, rctx->tot_len, wr_compl_cb, rctx)) != USBD_STATUS_SUCCESS) {
		TRACE_WARNING("USB EP busy while re-filling USB DMA: %d\n\r", res);
		req_ctx_set_state(rctx, RCTX_STATE_FREE);
		usb_state.active = 0;
		return -EBUSY;
	}

	usb_state.active = 1;
	return 0;
}

/* user API: requests us to start transmitting data via USB IN EP */
void fastsource_start(void)
{
	if(USBD_GetState() != USBD_STATE_CONFIGURED)
		return;

	if (!usb_state.active)
		refill_dma();
}

/* Use every Nth sample for computing statistics.  At fpga.adc_clkdiv=2 we can
 * still do every sample (NTH=1) at 20MHz SSC clock.  Above that, we have to look
 * at a sub-set only and thus increase NTH */
#define NTH	8

/* iterate over all samples in a given rctx and generate statistics */
static void rctx_stats_add(struct req_ctx *rctx, struct rctx_stats *s)
{
	uint16_t *data16;
	int inited = 0;

	//for (data16 = rctx->data; data16 < rctx->data + rctx->tot_len; data16 += 2) {
	for (data16 = rctx->data; data16 < rctx->data + rctx->tot_len; data16 += NTH*2) {
		uint32_t sum = data16[0] + data16[1];
		uint16_t diff_i, diff_q, last_i, last_q;

		s->total++;

		switch (sum) {
		case 0xFFFF:
			s->sum_ffff++;
			break;
		case 0xFFFE:
			s->sum_fffe++;
			break;
		default:
			s->sum_other++;
			break;
		}

		if (inited) {
			diff_i = (uint16_t)(last_i - data16[0]);
			diff_q = (uint16_t)(last_q - data16[0]);

			switch (diff_i) {
			case 1*NTH:
				s->delta_1++;
				break;
			case 2*NTH:
				s->delta_2++;
				break;
			case 3*NTH:
				s->delta_3++;
				break;
			default:
				s->delta_other++;
			}
		}

		inited = 1;
		last_i = data16[0];
		last_q = data16[1];
	}

	if (s->total > 0xFFFF) {
		printf("%u (f=%u/e=%u/o=%u) (1=%u/2=%u/3=%u/o=%u)\n\r",
			s->total, s->sum_ffff, s->sum_fffe, s->sum_other,
			s->delta_1, s->delta_2, s->delta_3, s->delta_other);
		memset(s, 0, sizeof(*s));
	}
}

/* SSC DMA informs us about completion of filling one rctx */
void usb_submit_req_ctx(struct req_ctx *rctx)
{
	req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);

#ifdef FPGA_TEST_STATS
	rctx_stats_add(rctx, &usb_state.stats);
#endif
	//TRACE_INFO("USB rctx enqueue (%08x, %u/%u)\n\r", rctx, rctx->size, rctx->tot_len);
	req_ctx_enqueue(&usb_state.queue, rctx);

	fastsource_start();
}

/* callback */
void USBDCallbacks_RequestReceived(const USBGenericRequest *request)
{
	fastsource_req_hdlr(request);
}

void USBDDriverCallbacks_InterfaceSettingChanged(unsigned char interface,
						 unsigned char setting)
{
	printf("USB_IF_CHANGED(%u, %u)\n\r", interface, setting);

	if ((interface == AUDDLoopRecDriverDescriptors_STREAMINGIN)
	    && (setting == 0))
		LED_Clear(USBD_LEDOTHER);
	else
		LED_Set(USBD_LEDOTHER);
}

void fastsource_dump(void)
{
	struct req_ctx *rctx, *rctx2;

	printf("usb pending:");
	llist_for_each_entry_safe(rctx, rctx2, &usb_state.queue, list)
		printf(" %02d", req_ctx_num(rctx));
	printf("\n\r");
}
