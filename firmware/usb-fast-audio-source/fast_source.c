#include <stdlib.h>
#include <stdint.h>
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

#include <AUDDFastSourceDescriptors.h>
#include <fast_source.h>

extern const USBDDriverDescriptors auddFastSourceDriverDescriptors;
static unsigned char driver_interfaces[3];
static USBDDriver fast_source_driver;

#define EP_NR 6

/* callback */
void USBDCallbacks_RequestReceived(const USBGenericRequest *request)
{
	fastsource_req_hdlr(request);
}

void USBDDriverCallbacks_InterfaceSettingChanged(unsigned char interface,
						 unsigned char setting)
{
	if ((interface == AUDDLoopRecDriverDescriptors_STREAMING)
	    && (setting == 0))
		LED_Clear(USBD_LEDOTHER);
	else
		LED_Set(USBD_LEDOTHER);
}

static void fastsource_get_feat_cur_val(uint8_t entity, uint8_t channel,
				   uint8_t control, uint8_t length)
{
	/* FIXME */
	USBD_Stall(0);
}

static void fastsource_set_feat_cur_val(uint8_t entity, uint8_t channel,
				   uint8_t control, uint8_t length)
{
	/* FIXME */
	USBD_Stall(0);
}

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

void fastsource_init(void)
{
	USBDDriver_Initialize(&fast_source_driver, &auddFastSourceDriverDescriptors,
				driver_interfaces);

	USBD_Init();
}

const uint8_t test_data[AUDDLoopRecDriver_BYTESPERFRAME] = { 
				0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
				0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
				0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
				0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f };

static void wr_compl_cb(void *arg, unsigned char status, unsigned int transferred,
			unsigned int remain)
{
	//TRACE_INFO("COMPL: status %u transferred %u remain %u\r\n", status, transferred, remain);
	//TRACE_INFO(".");

	if (status == 0 && remain == 0) {
		fastsource_start();
	} else {
		TRACE_WARNING("Err: EP%u wr_compl, status 0x%02u, xfr %u, remain %u\r\n",
				EP_NR, status, transferred, remain);
	}
}

void fastsource_start(void)
{
	//TRACE_WARNING("DATA: %02x %02x %02x %02x\r\n", test_data[0], test_data[1], test_data[2], test_data[3]);
	USBD_Write(EP_NR, test_data, sizeof(test_data), wr_compl_cb, NULL);
}

void fastsource_dump(void)
{
	printf("usb pending: ");
	printf("ssc pending:");
	llist_for_each_entry_safe(rctx, rctx2, &ssc_state.pending_rctx, list)
		printf(" %d", req_ctx_num(rctx));
	printf("\r\n");
}
