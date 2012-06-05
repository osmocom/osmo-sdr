
#include <stdint.h>

#define AUDDLoopRecDriver_SAMPLERATE	64000
#define	AUDDLoopRecDriver_NUMCHANNELS	2
#define AUDDLoopRecDriver_BYTESPERSAMPLE 2

#define AUDDLoopRecDriver_SAMPLESPERFRAME (AUDDLoopRecDriver_SAMPLERATE / 500 \
					   * AUDDLoopRecDriver_NUMCHANNELS)

#define AUDDLoopRecDriver_BYTESPERFRAME (AUDDLoopRecDriver_SAMPLESPERFRAME * \
					 AUDDLoopRecDriver_BYTESPERSAMPLE)

#include <usb/common/core/USBGenericRequest.h>

void fastsource_init(void);
void fastsource_start(void);
void fastsource_req_hdlr(const USBGenericRequest *request);
void fastsource_dump(void);
