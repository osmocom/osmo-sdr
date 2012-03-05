
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


#include <fast_source_descr.h>
#include <fast_source.h>

extern const USBDDriverDescriptors auddFastSourceDriverDescriptors;
unsigned char fastsource_interfaces[3];
static USBDDriver fast_source_driver;

struct usb_state {
	struct llist_head queue;
	int active;
	uint8_t muted;
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

	rctx = req_ctx_dequeue(&usb_state.queue);
	if (!rctx) {
		//TRACE_WARNING("No rctx for re-filling USB DMA\n\r");
		usb_state.active = 0;
		return -ENOENT;
	}

	req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_BUSY);

	if (USBD_Write(EP_NR, rctx->data, rctx->tot_len, wr_compl_cb,
			rctx) != USBD_STATUS_SUCCESS) {
		TRACE_WARNING("USB EP busy while re-filling USB DMA\n\r");
		usb_state.active = 0;
		return -EBUSY;
	}

	usb_state.active = 1;
	return 0;
}

/* user API: requests us to start transmitting data via USB IN EP */
void fastsource_start(void)
{
	if (!usb_state.active) {
		usb_state.active = 1;
		refill_dma();
	}
}

/* SSC DMA informs us about completion of filling one rctx */
void usb_submit_req_ctx(struct req_ctx *rctx)
{
	req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);

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
