
#include <stdint.h>

#include "req_ctx.h"

/* Some thoughts:
 * If we're running at 1MS/s, and the USB-HS microframe occurs every 125uS, we
 * need to transfer 125 samples at eech microframe.  125 samples with 2
 * channels of 2 bytes equals 500 bytes.
 */

#define AUDDLoopRecDriver_SAMPLERATE	500000
#define	AUDDLoopRecDriver_NUMCHANNELS	2
#define AUDDLoopRecDriver_BYTESPERSAMPLE 2

#if 0
#define AUDDLoopRecDriver_SAMPLESPERFRAME (AUDDLoopRecDriver_SAMPLERATE / 8000 \
					   * AUDDLoopRecDriver_NUMCHANNELS)

#else
#define AUDDLoopRecDriver_SAMPLESPERFRAME (128 * AUDDLoopRecDriver_NUMCHANNELS)
#endif

#define AUDDLoopRecDriver_BYTESPERFRAME (AUDDLoopRecDriver_SAMPLESPERFRAME * \
					 AUDDLoopRecDriver_BYTESPERSAMPLE)

#include <usb/common/core/USBGenericRequest.h>

extern unsigned char fastsource_interfaces[3];

void fastsource_init(void);
void fastsource_start(void);
void fastsource_dump(void);

void usb_submit_req_ctx(struct req_ctx *rctx);
