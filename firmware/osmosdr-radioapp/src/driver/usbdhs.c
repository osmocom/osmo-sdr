/* This file uses lots of code from the Atmel reference library:
 * ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2008, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

#include "usbdhs.h"
#include "../at91sam3u4/AT91SAM3U4.h"
#include "../board.h"
#include "../at91sam3u4/core_cm3.h"
#include "../crt/stdio.h"
#include "../usb/usbdevice.h"
#include "irq.h"

#define DMA

/// Endpoint states: Endpoint is disabled
#define UDP_ENDPOINT_DISABLED       0
/// Endpoint states: Endpoint is halted (i.e. STALLs every request)
#define UDP_ENDPOINT_HALTED         1
/// Endpoint states: Endpoint is idle (i.e. ready for transmission)
#define UDP_ENDPOINT_IDLE           2
/// Endpoint states: Endpoint is sending data
#define UDP_ENDPOINT_SENDING        3
/// Endpoint states: Endpoint is receiving data
#define UDP_ENDPOINT_RECEIVING      4
/// Endpoint states: Endpoint is sending MBL
#define UDP_ENDPOINT_SENDINGM       5
/// Endpoint states: Endpoint is receiving MBL
#define UDP_ENDPOINT_RECEIVINGM     6

#define NUM_IT_MAX       \
    (AT91C_BASE_UDPHS->UDPHS_IPFEATURES & AT91C_UDPHS_EPT_NBR_MAX)
#define SHIFT_INTERUPT    8
#define SHIFT_DMA        24

#define EPT_VIRTUAL_SIZE      16384
#define DMA_MAX_FIFO_SIZE     65536

#define UDP_IsDmaUsed(ep)   (CHIP_USB_ENDPOINTS_DMA(ep))

typedef void (*MblTransferCallback)(void *pArg, unsigned char status, unsigned int nbFreed);

typedef struct _USBDTransferBuffer {
	/// Pointer to frame buffer
	unsigned char * pBuffer;
	/// Size of the frame (up to 64K-1)
	unsigned short size;
	/// Bytes transferred
	unsigned short transferred;
	/// Bytes in FIFO
	unsigned short buffered;
	/// Bytes remaining
	unsigned short remaining;
} USBDTransferBuffer;

typedef struct {
	/// Pointer to a data buffer used for emission/reception.
	char *pData;
	/// Number of bytes which have been written into the UDP internal FIFO
	/// buffers.
	int buffered;
	/// Number of bytes which have been sent/received.
	int transferred;
	/// Number of bytes which have not been buffered/transferred yet.
	int remaining;
	/// Optional callback to invoke when the transfer completes.
	TransferCallback fCallback;
	/// Optional argument to the callback function.
	void *pArgument;
} Transfer;

typedef struct _USBDDmaDescriptor {
	/// Pointer to Next Descriptor
	void* pNxtDesc;
	/// Pointer to data buffer address
	void* pDataAddr;
	/// DMA Control setting register value
	unsigned int ctrlSettings :8, /// Control settings
	    reserved :8, /// Not used
	    bufferLength :16; /// Length of buffer
	/// Loaded to DMA register, OK to modify
	unsigned int used;
}__attribute__((aligned(16))) USBDDmaDescriptor;

/// Describes Multi Buffer List transfer on a UDP endpoint.
typedef struct {
	/// Pointer to frame list
	USBDTransferBuffer *pMbl;
	/// Pointer to last loaded buffer
	USBDTransferBuffer *pLastLoaded;
	/// List size
	unsigned short listSize;
	/// Current processing frame
	unsigned short currBuffer;
	/// First freed frame to re-use
	unsigned short freedBuffer;
	/// Frame setting, circle frame list
	unsigned char circList;
	/// All buffer listed is used
	unsigned char allUsed;
	/// Optional callback to invoke when the transfer completes.
	MblTransferCallback fCallback;
	/// Optional argument to the callback function.
	void *pArgument;
} MblTransfer;

/// Describes DMA Link List transfer on a UDPHS endpoint.
typedef struct {
	/// Pointer to frame list
	USBDDmaDescriptor *pMbl;
	/// Pointer to current buffer
	USBDDmaDescriptor *pCurrBuffer;
	/// Pointer to freed buffer
	USBDDmaDescriptor *pFreedBuffer;
	/// List size
	unsigned short listSize;
	/// Frame setting, circle frame list
	unsigned char circList;
	/// All buffer listed is used
	unsigned char allUsed;
	/// Optional callback to invoke when the transfer completes.
	MblTransferCallback fCallback;
	/// Optional argument to the callback function.
	void *pArgument;
} DmaLlTransfer;

typedef struct {
	/// Current endpoint state.
	volatile unsigned char state;
	/// Current reception bank (0 or 1).
	unsigned char bank;
	/// Maximum packet size for the endpoint.
	unsigned short size;
	/// Describes an ongoing transfer (if current state is either
	///  <UDP_ENDPOINT_SENDING> or <UDP_ENDPOINT_RECEIVING>)
	union {
		Transfer singleTransfer;
		MblTransfer mblTransfer;
		DmaLlTransfer dmaTransfer;
	} transfer;
	/// Special case for send a ZLP
	unsigned char sendZLP;
} Endpoint;

static Endpoint endpoints[CHIP_USB_NUMENDPOINTS];

/// Device current state.
static unsigned char deviceState;
/// Indicates the previous device state
static unsigned char previousDeviceState;

static inline void UDPHS_DisableBIAS(void)
{
	AT91C_BASE_PMC->PMC_UCKR &= ~(unsigned int)AT91C_CKGR_BIASEN_ENABLED;
}

static inline void UDPHS_EnableBIAS(void)
{
	UDPHS_DisableBIAS();
	AT91C_BASE_PMC->PMC_UCKR |= AT91C_CKGR_BIASEN_ENABLED;
}

static void UDPHS_EnableUsbClock(void)
{
	AT91C_BASE_PMC->PMC_PCER = (1 << AT91C_ID_UDPHS);
	// Enable 480MHZ
	//AT91C_BASE_CKGR->CKGR_UCKR |= (AT91C_CKGR_PLLCOUNT & (3 << 20)) | AT91C_CKGR_UPLLEN;
	AT91C_BASE_CKGR->CKGR_UCKR |= ((0xf << 20) & (3 << 20)) | AT91C_CKGR_UPLLEN;
	// Wait until UTMI PLL is locked
	while((AT91C_BASE_PMC->PMC_SR & AT91C_PMC_LOCKU) == 0)
		;
}

static void UDPHS_DisableUsbClock(void)
{
	AT91C_BASE_PMC->PMC_PCDR = (1 << AT91C_ID_UDPHS);
	// 480MHZ
	AT91C_BASE_CKGR->CKGR_UCKR &= ~(unsigned int)AT91C_CKGR_UPLLEN;
}

static void UDPHS_ResetEndpoints(void)
{
	Endpoint *pEndpoint;
	Transfer *pTransfer;
	unsigned char bEndpoint;

	// Reset the transfer descriptor of every endpoint
	for(bEndpoint = 0; bEndpoint < CHIP_USB_NUMENDPOINTS; bEndpoint++) {

		pEndpoint = &(endpoints[bEndpoint]);
		pTransfer = (Transfer*)&(pEndpoint->transfer);

		// Reset endpoint transfer descriptor
		pTransfer->pData = 0;
		pTransfer->transferred = -1;
		pTransfer->buffered = -1;
		pTransfer->remaining = -1;
		pTransfer->fCallback = 0;
		pTransfer->pArgument = 0;

		// Reset endpoint state
		pEndpoint->bank = 0;
		pEndpoint->state = UDP_ENDPOINT_DISABLED;
		// Reset ZLP
		pEndpoint->sendZLP = 0;
	}
}

static void UDPHS_EndOfTransfer(unsigned char bEndpoint, char bStatus)
{
	Endpoint *pEndpoint = &(endpoints[bEndpoint]);
	Transfer *pTransfer = (Transfer*)&(pEndpoint->transfer);

	// Check that endpoint was sending or receiving data
	if((pEndpoint->state == UDP_ENDPOINT_RECEIVING) || (pEndpoint->state == UDP_ENDPOINT_SENDING)) {
		if(pEndpoint->state == UDP_ENDPOINT_SENDING) {
			pEndpoint->sendZLP = 0;
		}
		// Endpoint returns in Idle state
		pEndpoint->state = UDP_ENDPOINT_IDLE;

		// Invoke callback is present
		if(pTransfer->fCallback != 0) {

			((TransferCallback)pTransfer->fCallback)(pTransfer->pArgument, bStatus, pTransfer->transferred, pTransfer->remaining + pTransfer->buffered);
		}
	} else if((pEndpoint->state == UDP_ENDPOINT_RECEIVINGM) || (pEndpoint->state == UDP_ENDPOINT_SENDINGM)) {

		MblTransfer* pMblt = (MblTransfer*)&(pEndpoint->transfer);
		MblTransferCallback fCallback;
		void* pArg;

#ifdef DMA
		{
			DmaLlTransfer *pDmat = (DmaLlTransfer*)&(pEndpoint->transfer);
			fCallback = UDP_IsDmaUsed(bEndpoint) ? pDmat->fCallback : pMblt->fCallback;
			pArg = UDP_IsDmaUsed(bEndpoint) ? pDmat->pArgument : pMblt->pArgument;
		}
#else
		fCallback = pMblt->fCallback;
		pArg = pMblt->pArgument;
#endif

		// Endpoint returns in Idle state
		pEndpoint->state = UDP_ENDPOINT_IDLE;
		// Invoke callback
		if(fCallback != 0) {

			(fCallback)(pArg, bStatus, 0);
		}
	}
}

static void UDPHS_DisableEndpoints(void)
{
	unsigned char bEndpoint;

	// Disable each endpoint, terminating any pending transfer
	// Control endpoint 0 is not disabled
	for(bEndpoint = 1; bEndpoint < CHIP_USB_NUMENDPOINTS; bEndpoint++) {

		UDPHS_EndOfTransfer(bEndpoint, USBD_STATUS_ABORTED);
		endpoints[bEndpoint].state = UDP_ENDPOINT_DISABLED;
	}
}

void usbdhs_configureEndpoint(const USBEndpointDescriptor* pDescriptor)
{
	Endpoint *pEndpoint;
	unsigned char bEndpoint;
	unsigned char bType;
	unsigned char bEndpointDir;
	//unsigned char bInterval = 0;
	unsigned char bNbTrans = 1;
	unsigned char bSizeEpt = 0;
	unsigned char bHs = ((AT91C_BASE_UDPHS->UDPHS_INTSTA & AT91C_UDPHS_SPEED) > 0);

	// NULL descriptor -> Control endpoint 0
	if(pDescriptor == 0) {

		bEndpoint = 0;
		pEndpoint = &(endpoints[bEndpoint]);
		bType = USBEndpointDescriptor_CONTROL;
		bEndpointDir = 0;
		pEndpoint->size = CHIP_USB_ENDPOINTS_MAXPACKETSIZE(0);
		pEndpoint->bank = CHIP_USB_ENDPOINTS_BANKS(0);
	} else {

		// The endpoint number
		bEndpoint = usbEndpointDescriptor_GetNumber(pDescriptor);
		pEndpoint = &(endpoints[bEndpoint]);
		// Transfer type: Control, Isochronous, Bulk, Interrupt
		bType = usbEndpointDescriptor_GetType(pDescriptor);
		// interval
		//bInterval = USBEndpointDescriptor_GetInterval(pDescriptor);
		// Direction, ignored for control endpoints
		bEndpointDir = usbEndpointDescriptor_GetDirection(pDescriptor);
		pEndpoint->size = usbEndpointDescriptor_GetMaxPacketSize(pDescriptor);
		pEndpoint->bank = CHIP_USB_ENDPOINTS_BANKS(bEndpoint);

		// Convert descriptor value to EP configuration
		if(bHs) { // HS Interval, *125us

			// MPS: Bit12,11 specify NB_TRANS
			bNbTrans = ((pEndpoint->size >> 11) & 0x3);
			if(bNbTrans == 3)
				bNbTrans = 1;
			else bNbTrans++;

			// Mask, bit 10..0 is the size
			pEndpoint->size &= 0x3FF;
		}
	}

	// Abort the current transfer is the endpoint was configured and in
	// Write or Read state
	if((pEndpoint->state == UDP_ENDPOINT_RECEIVING) || (pEndpoint->state == UDP_ENDPOINT_SENDING) || (pEndpoint->state == UDP_ENDPOINT_RECEIVINGM)
	    || (pEndpoint->state == UDP_ENDPOINT_SENDINGM)) {

		UDPHS_EndOfTransfer(bEndpoint, USBD_STATUS_RESET);
	}
	pEndpoint->state = UDP_ENDPOINT_IDLE;

	// Disable endpoint
	AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCTLDIS = (unsigned int)AT91C_UDPHS_SHRT_PCKT | AT91C_UDPHS_BUSY_BANK | AT91C_UDPHS_NAK_OUT
	    | AT91C_UDPHS_NAK_IN | AT91C_UDPHS_STALL_SNT | AT91C_UDPHS_RX_SETUP | AT91C_UDPHS_TX_PK_RDY | AT91C_UDPHS_TX_COMPLT | AT91C_UDPHS_RX_BK_RDY
	    | AT91C_UDPHS_ERR_OVFLW | AT91C_UDPHS_MDATA_RX | AT91C_UDPHS_DATAX_RX | AT91C_UDPHS_NYET_DIS | AT91C_UDPHS_INTDIS_DMA | AT91C_UDPHS_AUTO_VALID
	    | AT91C_UDPHS_EPT_DISABL;
	// Reset Endpoint Fifos
	AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCLRSTA = AT91C_UDPHS_TOGGLESQ | AT91C_UDPHS_FRCESTALL;
	AT91C_BASE_UDPHS->UDPHS_EPTRST = 1 << bEndpoint;

	// Configure endpoint
	if(pEndpoint->size <= 8) {
		bSizeEpt = 0;
	} else if(pEndpoint->size <= 16) {
		bSizeEpt = 1;
	} else if(pEndpoint->size <= 32) {
		bSizeEpt = 2;
	} else if(pEndpoint->size <= 64) {
		bSizeEpt = 3;
	} else if(pEndpoint->size <= 128) {
		bSizeEpt = 4;
	} else if(pEndpoint->size <= 256) {
		bSizeEpt = 5;
	} else if(pEndpoint->size <= 512) {
		bSizeEpt = 6;
	} else if(pEndpoint->size <= 1024) {
		bSizeEpt = 7;
	} //else {
	  //  sizeEpt = 0; // control endpoint
	  //}

	// Configure endpoint
	if(bType == USBEndpointDescriptor_CONTROL) {

		// Enable endpoint IT for control endpoint
		AT91C_BASE_UDPHS->UDPHS_IEN |= (1 << SHIFT_INTERUPT << bEndpoint);
	}

	AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCFG = bSizeEpt | (bEndpointDir << 3) | (bType << 4) | ((pEndpoint->bank) << 6) | (bNbTrans << 8);

	while((signed int)AT91C_UDPHS_EPT_MAPD != (signed int)((AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCFG) & (unsigned int)AT91C_UDPHS_EPT_MAPD)) {

		// resolved by clearing the reset IT in good place
		/*
		 TRACE_ERROR("PB bEndpoint: 0x%X\n\r", bEndpoint);
		 TRACE_ERROR("PB bSizeEpt: 0x%X\n\r", bSizeEpt);
		 TRACE_ERROR("PB bEndpointDir: 0x%X\n\r", bEndpointDir);
		 TRACE_ERROR("PB bType: 0x%X\n\r", bType);
		 TRACE_ERROR("PB pEndpoint->bank: 0x%X\n\r", pEndpoint->bank);
		 TRACE_ERROR("PB UDPHS_EPTCFG: 0x%X\n\r", AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCFG);
		 */
		for(;;)
			;
	}

	if(bType == USBEndpointDescriptor_CONTROL) {

		AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCTLENB = AT91C_UDPHS_RX_BK_RDY | AT91C_UDPHS_RX_SETUP | AT91C_UDPHS_EPT_ENABL;
	} else {
#ifndef DMA
		AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCTLENB = AT91C_UDPHS_EPT_ENABL;
#else
		AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCTLENB = AT91C_UDPHS_AUTO_VALID | AT91C_UDPHS_EPT_ENABL;
#endif
	}

}

static char UDPHS_MblUpdate(MblTransfer *pTransfer, USBDTransferBuffer * pBi, unsigned short size, unsigned char forceEnd)
{
	// Update transfer descriptor
	pBi->remaining -= size;
	// Check if current buffer ended
	if(pBi->remaining == 0 || forceEnd) {

		// Process to next buffer
		if((++pTransfer->currBuffer) == pTransfer->listSize) {
			if(pTransfer->circList) {
				pTransfer->currBuffer = 0;
			} else {
				pTransfer->allUsed = 1;
			}
		}
		// All buffer in the list is processed
		if(pTransfer->currBuffer == pTransfer->freedBuffer) {
			pTransfer->allUsed = 1;
		}
		// Continue transfer, prepare for next operation
		if(pTransfer->allUsed == 0) {
			pBi = &pTransfer->pMbl[pTransfer->currBuffer];
			pBi->buffered = 0;
			pBi->transferred = 0;
			pBi->remaining = pBi->size;
		}
		return 1;
	}
	return 0;
}

static char UDPHS_MblWriteFifo(unsigned char bEndpoint)
{
	Endpoint *pEndpoint = &(endpoints[bEndpoint]);
	MblTransfer *pTransfer = (MblTransfer*)&(pEndpoint->transfer);
	USBDTransferBuffer *pBi = &(pTransfer->pMbl[pTransfer->currBuffer]);
	volatile char *pFifo;
	signed int size;

	volatile unsigned char * pBytes;
	volatile char bufferEnd = 1;

	// Get the number of bytes to send
	size = pEndpoint->size;
	if(size > pBi->remaining) {

		size = pBi->remaining;
	}

	if(size == 0) {

		return 1;
	}

	pTransfer->pLastLoaded = pBi;
	pBytes = &(pBi->pBuffer[pBi->transferred + pBi->buffered]);
	pBi->buffered += size;
	bufferEnd = UDPHS_MblUpdate(pTransfer, pBi, size, 0);

	// Write packet in the FIFO buffer
	pFifo = (char*)((unsigned int *)AT91C_BASE_UDPHS_EPTFIFO + (EPT_VIRTUAL_SIZE * bEndpoint));
	if(size) {
		signed int c8 = size >> 3;
		signed int c1 = size & 0x7;
		//printf("%d[%x] ", pBi->transferred, pBytes);
		for(; c8; c8--) {
			*(pFifo++) = *(pBytes++);
			*(pFifo++) = *(pBytes++);
			*(pFifo++) = *(pBytes++);
			*(pFifo++) = *(pBytes++);

			*(pFifo++) = *(pBytes++);
			*(pFifo++) = *(pBytes++);
			*(pFifo++) = *(pBytes++);
			*(pFifo++) = *(pBytes++);
		}
		for(; c1; c1--) {
			*(pFifo++) = *(pBytes++);
		}
	}
	return bufferEnd;
}

static void UDPHS_WritePayload(unsigned char bEndpoint)
{
	Endpoint *pEndpoint = &(endpoints[bEndpoint]);
	Transfer *pTransfer = (Transfer*)&(pEndpoint->transfer);
	char *pFifo;
	signed int size;
	unsigned int dCtr;

	pFifo = (char*)((unsigned int *)AT91C_BASE_UDPHS_EPTFIFO + (EPT_VIRTUAL_SIZE * bEndpoint));

	// Get the number of bytes to send
	size = pEndpoint->size;
	if(size > pTransfer->remaining) {

		size = pTransfer->remaining;
	}

	// Update transfer descriptor information
	pTransfer->buffered += size;
	pTransfer->remaining -= size;

	// Write packet in the FIFO buffer
	dCtr = 0;
	while(size > 0) {

		pFifo[dCtr] = *(pTransfer->pData);
		pTransfer->pData++;
		size--;
		dCtr++;
	}
}

static void UDPHS_ReadPayload(unsigned char bEndpoint, int wPacketSize)
{
	Endpoint *pEndpoint = &(endpoints[bEndpoint]);
	Transfer *pTransfer = (Transfer*)&(pEndpoint->transfer);
	char *pFifo;
	unsigned char dBytes = 0;

	pFifo = (char*)((unsigned int *)AT91C_BASE_UDPHS_EPTFIFO + (EPT_VIRTUAL_SIZE * bEndpoint));

	// Check that the requested size is not bigger than the remaining transfer
	if(wPacketSize > pTransfer->remaining) {

		pTransfer->buffered += wPacketSize - pTransfer->remaining;
		wPacketSize = pTransfer->remaining;
	}

	// Update transfer descriptor information
	pTransfer->remaining -= wPacketSize;
	pTransfer->transferred += wPacketSize;

	// Retrieve packet
	while(wPacketSize > 0) {

		*(pTransfer->pData) = pFifo[dBytes];
		pTransfer->pData++;
		wPacketSize--;
		dBytes++;
	}
}

static void UDPHS_ReadRequest(USBGenericRequest *pRequest)
{
	unsigned int *pData = (unsigned int *)pRequest;
	unsigned int fifo;

	fifo = (AT91C_BASE_UDPHS_EPTFIFO->UDPHS_READEPT0[0]);
	*pData = fifo;
	fifo = (AT91C_BASE_UDPHS_EPTFIFO->UDPHS_READEPT0[0]);
	pData++;
	*pData = fifo;
	//TRACE_ERROR("SETUP: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n\r", pData[0],pData[1],pData[2],pData[3],pData[4],pData[5],pData[6],pData[7]);
}

static void UDPHS_ClearRxFlag(unsigned char bEndpoint)
{
	AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCLRSTA = AT91C_UDPHS_RX_BK_RDY;
}

static void UDPHS_EndpointHandler(unsigned char bEndpoint)
{
	Endpoint *pEndpoint = &(endpoints[bEndpoint]);
	Transfer *pTransfer = (Transfer*)&(pEndpoint->transfer);
	MblTransfer *pMblt = (MblTransfer*)&(pEndpoint->transfer);
	unsigned int status = AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTSTA;
	unsigned short wPacketSize;
	USBGenericRequest request;

	// Handle interrupts
	// IN packet sent
	if((AT91C_UDPHS_TX_PK_RDY == (AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCTL & AT91C_UDPHS_TX_PK_RDY)) && (0 == (status & AT91C_UDPHS_TX_PK_RDY))) {
		// Check that endpoint was sending multi-buffer-list
		if(pEndpoint->state == UDP_ENDPOINT_SENDINGM) {

			USBDTransferBuffer * pMbli = pMblt->pLastLoaded;

			// End of transfer ?
			if(pMblt->allUsed && pMbli->remaining == 0) {

				pMbli->transferred += pMbli->buffered;
				pMbli->buffered = 0;

				// Disable interrupt
				AT91C_BASE_UDPHS->UDPHS_IEN &= ~(1 << SHIFT_INTERUPT << bEndpoint);
				AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCTLDIS = AT91C_UDPHS_TX_PK_RDY;
				UDPHS_EndOfTransfer(bEndpoint, USBD_STATUS_SUCCESS);
			} else {
				if(pMbli->buffered > pEndpoint->size) {
					pMbli->transferred += pEndpoint->size;
					pMbli->buffered -= pEndpoint->size;
				} else {
					pMbli->transferred += pMbli->buffered;
					pMbli->buffered = 0;
				}

				// Send next packet
				UDPHS_MblWriteFifo(bEndpoint);
				AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTSETSTA = AT91C_UDPHS_TX_PK_RDY;

				if(pMbli->remaining == 0 && pMbli->buffered == 0) {

					if(pMblt->fCallback) {
						((MblTransferCallback)pTransfer->fCallback)(pTransfer->pArgument, USBD_STATUS_PARTIAL_DONE, 1);
					}
				}
			}
		}
		// Check that endpoint was in Sending state
		if(pEndpoint->state == UDP_ENDPOINT_SENDING) {

			if(pTransfer->buffered > 0) {
				pTransfer->transferred += pTransfer->buffered;
				pTransfer->buffered = 0;
			}

			if(((pTransfer->buffered) == 0) && ((pTransfer->transferred) == 0) && ((pTransfer->remaining) == 0) && (pEndpoint->sendZLP == 0)) {
				pEndpoint->sendZLP = 1;
			}

			// End of transfer ?
			if((pTransfer->remaining > 0) || (pEndpoint->sendZLP == 1)) {

				pEndpoint->sendZLP = 2;

				// Send next packet
				UDPHS_WritePayload(bEndpoint);
				AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTSETSTA = AT91C_UDPHS_TX_PK_RDY;
			} else {
				// Disable interrupt if this is not a control endpoint
				if(AT91C_UDPHS_EPT_TYPE_CTL_EPT != (AT91C_UDPHS_EPT_TYPE & (AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCFG))) {

					AT91C_BASE_UDPHS->UDPHS_IEN &= ~(1 << SHIFT_INTERUPT << bEndpoint);
				}
				AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCTLDIS = AT91C_UDPHS_TX_PK_RDY;

				UDPHS_EndOfTransfer(bEndpoint, USBD_STATUS_SUCCESS);
				pEndpoint->sendZLP = 0;
			}
		}
	}
	// OUT packet received
	if(AT91C_UDPHS_RX_BK_RDY == (status & AT91C_UDPHS_RX_BK_RDY)) {

		// Check that the endpoint is in Receiving state
		if(pEndpoint->state != UDP_ENDPOINT_RECEIVING) {

			// Check if an ACK has been received on a Control endpoint
			if((0 == (AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCFG & AT91C_UDPHS_EPT_TYPE)) && (0 == (status & AT91C_UDPHS_BYTE_COUNT))) {

				// Control endpoint, 0 bytes received
				// Acknowledge the data and finish the current transfer
				UDPHS_ClearRxFlag(bEndpoint);
				UDPHS_EndOfTransfer(bEndpoint, USBD_STATUS_SUCCESS);
				//todo remove endoftranfer and test
			}
			// Check if the data has been STALLed
			else if(AT91C_UDPHS_FRCESTALL == (status & AT91C_UDPHS_FRCESTALL)) {

				// Discard STALLed data
				UDPHS_ClearRxFlag(bEndpoint);
			}
			// NAK the data
			else {
				AT91C_BASE_UDPHS->UDPHS_IEN &= ~(1 << SHIFT_INTERUPT << bEndpoint);
			}
		} else {

			// Endpoint is in Read state
			// Retrieve data and store it into the current transfer buffer
			wPacketSize = (unsigned short)((status & AT91C_UDPHS_BYTE_COUNT) >> 20);

			UDPHS_ReadPayload(bEndpoint, wPacketSize);
			UDPHS_ClearRxFlag(bEndpoint);

			// Check if the transfer is finished
			if((pTransfer->remaining == 0) || (wPacketSize < pEndpoint->size)) {

				AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCTLDIS = AT91C_UDPHS_RX_BK_RDY;

				// Disable interrupt if this is not a control endpoint
				if(AT91C_UDPHS_EPT_TYPE_CTL_EPT != (AT91C_UDPHS_EPT_TYPE & (AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCFG))) {

					AT91C_BASE_UDPHS->UDPHS_IEN &= ~(1 << SHIFT_INTERUPT << bEndpoint);
				}
				UDPHS_EndOfTransfer(bEndpoint, USBD_STATUS_SUCCESS);
			}
		}
	}

	// STALL sent
	if(AT91C_UDPHS_STALL_SNT == (status & AT91C_UDPHS_STALL_SNT)) {
		// Acknowledge the stall flag
		AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCLRSTA = AT91C_UDPHS_STALL_SNT;

		// If the endpoint is not halted, clear the STALL condition
		if(pEndpoint->state != UDP_ENDPOINT_HALTED) {
			AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCLRSTA = AT91C_UDPHS_FRCESTALL;
		}
	}
	// SETUP packet received
	if(AT91C_UDPHS_RX_SETUP == (status & AT91C_UDPHS_RX_SETUP)) {
		// If a transfer was pending, complete it
		// Handles the case where during the status phase of a control write
		// transfer, the host receives the device ZLP and ack it, but the ack
		// is not received by the device
		if((pEndpoint->state == UDP_ENDPOINT_RECEIVING) || (pEndpoint->state == UDP_ENDPOINT_SENDING)) {

			UDPHS_EndOfTransfer(bEndpoint, USBD_STATUS_SUCCESS);
		}
		// Copy the setup packet
		UDPHS_ReadRequest(&request);

		// Acknowledge setup packet
		AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCLRSTA = AT91C_UDPHS_RX_SETUP;

		// Forward the request to the upper layer
		usbDevice_requestReceived(&request);
	}
}

static void UDPHS_DmaHandler(unsigned char bEndpoint)
{
	Endpoint *pEndpoint = &(endpoints[bEndpoint]);
	Transfer *pTransfer = (Transfer*)&(pEndpoint->transfer);
	int justTransferred;
	unsigned int status;
	unsigned char result = USBD_STATUS_SUCCESS;

	status = AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMASTATUS;

	if(pEndpoint->state == UDP_ENDPOINT_SENDINGM) {

		DmaLlTransfer *pDmat = (DmaLlTransfer*)&(pEndpoint->transfer);
		USBDDmaDescriptor *pDi = pDmat->pCurrBuffer;
		USBDDmaDescriptor *pNi = (USBDDmaDescriptor*)AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMANXTDSC;
		unsigned char bufCnt = 0;

		if(AT91C_UDPHS_END_BF_ST == (status & AT91C_UDPHS_END_BF_ST)) {

			// Stop loading descriptors
			AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMACONTROL &= ~((unsigned int)AT91C_UDPHS_LDNXT_DSC);

			//printf("%x,%x|",
			//    (char)status,
			//    (short)AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMANXTDSC);

			while(pDi->pNxtDesc != pNi) {

				// All descriptor processed
				if(pDi == 0)
					break;
				if(pDi->used)
					break;

				// Current descriptor processed
				/*
				 pDi->ctrlSettings = 0
				 //|AT91C_UDPHS_CHANN_ENB
				 //| AT91C_UDPHS_LDNXT_DSC
				 //| AT91C_UDPHS_END_TR_EN
				 //| AT91C_UDPHS_END_TR_IT
				 //| AT91C_UDPHS_END_B_EN
				 //| AT91C_UDPHS_END_BUFFIT
				 //| AT91C_UDPHS_DESC_LD_IT
				 ; */
				pDi->used = 1; // Flag as used
				bufCnt++;
				pDi = pDi->pNxtDesc;
			}
			pDmat->pCurrBuffer = pDi;

			if(bufCnt) {

				if(pDmat->fCallback) {

					pDmat->fCallback(pDmat->pArgument, ((result == USBD_STATUS_SUCCESS) ? USBD_STATUS_PARTIAL_DONE : result), bufCnt);
				}
			}

			if((pNi == 0) || (pNi->used && pNi)) {

				// Disable DMA endpoint interrupt
				AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMACONTROL = 0;
				AT91C_BASE_UDPHS->UDPHS_IEN &= ~(1 << SHIFT_DMA << bEndpoint);
				//printf("E:N%x,C%d ", (short)pNi, bufCnt);

				UDPHS_EndOfTransfer(bEndpoint, result);
			} else {

				AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMACONTROL |= (AT91C_UDPHS_LDNXT_DSC);
			}
		} else {
			result = USBD_STATUS_ABORTED;
			UDPHS_EndOfTransfer(bEndpoint, result);
		}
		return;
	} else if(pEndpoint->state == UDP_ENDPOINT_RECEIVINGM) {
		return;
	}

	// Disable DMA interrupt to avoid receiving 2 interrupts (B_EN and TR_EN)
	AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMACONTROL &= ~(unsigned int)(AT91C_UDPHS_END_TR_EN | AT91C_UDPHS_END_B_EN);

	AT91C_BASE_UDPHS->UDPHS_IEN &= ~(1 << SHIFT_DMA << bEndpoint);

	if(AT91C_UDPHS_END_BF_ST == (status & AT91C_UDPHS_END_BF_ST)) {

		// BUFF_COUNT holds the number of untransmitted bytes.
		// BUFF_COUNT is equal to zero in case of good transfer
		justTransferred = pTransfer->buffered - ((status & (unsigned int)AT91C_UDPHS_BUFF_COUNT) >> 16);
		pTransfer->transferred += justTransferred;

		pTransfer->buffered = ((status & (unsigned int)AT91C_UDPHS_BUFF_COUNT) >> 16);

		pTransfer->remaining -= justTransferred;

		if((pTransfer->remaining + pTransfer->buffered) > 0) {

			// Prepare an other transfer
			if(pTransfer->remaining > DMA_MAX_FIFO_SIZE) {

				pTransfer->buffered = DMA_MAX_FIFO_SIZE;
			} else {
				pTransfer->buffered = pTransfer->remaining;
			}

			AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMAADDRESS = (unsigned int)((pTransfer->pData) + (pTransfer->transferred));

			// Clear unwanted interrupts
			AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMASTATUS;

			// Enable DMA endpoint interrupt
			AT91C_BASE_UDPHS->UDPHS_IEN |= (1 << SHIFT_DMA << bEndpoint);
			// DMA config for receive the good size of buffer, or an error buffer

			AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMACONTROL = 0; // raz
			AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMACONTROL = (((pTransfer->buffered << 16) & AT91C_UDPHS_BUFF_COUNT) | AT91C_UDPHS_END_TR_EN
			    | AT91C_UDPHS_END_TR_IT | AT91C_UDPHS_END_B_EN | AT91C_UDPHS_END_BUFFIT | AT91C_UDPHS_CHANN_ENB);
		}
	} else if(AT91C_UDPHS_END_TR_ST == (status & AT91C_UDPHS_END_TR_ST)) {
		pTransfer->transferred = pTransfer->buffered - ((status & (unsigned int)AT91C_UDPHS_BUFF_COUNT) >> 16);
		pTransfer->remaining = 0;
	} else {
		result = USBD_STATUS_ABORTED;
	}
	// Invoke callback
	if(pTransfer->remaining == 0) {
		UDPHS_EndOfTransfer(bEndpoint, result);
	}
}

void usbd_irqHandler(void)
{
	unsigned int status;
	unsigned char numIT;
	/*
	 if(deviceState >= USBD_STATE_POWERED) {

	 LED_Set(USBD_LEDUSB);
	 }
	 */
	// Get interrupts status
	status = AT91C_BASE_UDPHS->UDPHS_INTSTA;
	status &= AT91C_BASE_UDPHS->UDPHS_IEN;

	// Handle all UDPHS interrupts
	while(status != 0) {

		// Start Of Frame (SOF)
		if((status & AT91C_UDPHS_IEN_SOF) != 0) {
			// Invoke the SOF callback
			//USB_StartOfFrameCallback(pUsb);

			// Acknowledge interrupt
			AT91C_BASE_UDPHS->UDPHS_CLRINT = AT91C_UDPHS_IEN_SOF;
			status &= ~(unsigned int)AT91C_UDPHS_IEN_SOF;
		}
		// Suspend
		// This interrupt is always treated last (hence the '==')
		else if(status == AT91C_UDPHS_DET_SUSPD) {
			// The device enters the Suspended state
			// MCK + UDPCK must be off
			// Pull-Up must be connected
			// Transceiver must be disabled

			//LED_Clear(USBD_LEDUSB);

			UDPHS_DisableBIAS();

			// Enable wakeup
			AT91C_BASE_UDPHS->UDPHS_IEN |= AT91C_UDPHS_WAKE_UP | AT91C_UDPHS_ENDOFRSM;
			AT91C_BASE_UDPHS->UDPHS_IEN &= ~(unsigned int)AT91C_UDPHS_DET_SUSPD;

			// Acknowledge interrupt
			AT91C_BASE_UDPHS->UDPHS_CLRINT = AT91C_UDPHS_DET_SUSPD | AT91C_UDPHS_WAKE_UP;
			previousDeviceState = deviceState;
			deviceState = USBD_STATE_SUSPENDED;
			UDPHS_DisableUsbClock();

			// Invoke the Suspend callback
			//USBDCallbacks_Suspended();
		}
		// Resume
		else if(((status & AT91C_UDPHS_WAKE_UP) != 0) // line activity
			|| ((status & AT91C_UDPHS_ENDOFRSM) != 0)) { // pc wakeup
			// Invoke the Resume callback
			//USBDCallbacks_Resumed();

			UDPHS_EnableUsbClock();
			UDPHS_EnableBIAS();

			// The device enters Configured state
			// MCK + UDPCK must be on
			// Pull-Up must be connected
			// Transceiver must be enabled

			deviceState = previousDeviceState;

			AT91C_BASE_UDPHS->UDPHS_CLRINT = AT91C_UDPHS_WAKE_UP | AT91C_UDPHS_ENDOFRSM | AT91C_UDPHS_DET_SUSPD;

			AT91C_BASE_UDPHS->UDPHS_IEN |= AT91C_UDPHS_ENDOFRSM | AT91C_UDPHS_DET_SUSPD;
			AT91C_BASE_UDPHS->UDPHS_CLRINT = AT91C_UDPHS_WAKE_UP | AT91C_UDPHS_ENDOFRSM;
			AT91C_BASE_UDPHS->UDPHS_IEN &= ~(unsigned int)AT91C_UDPHS_WAKE_UP;
		}
		// End of bus reset
		else if((status & AT91C_UDPHS_ENDRESET) == AT91C_UDPHS_ENDRESET) {
			// End of bus reset

			//            TRACE_DEBUG_WP("EoB ");

#if defined(BOARD_USB_DFU) && !defined(dfu)
			if (g_dfu.past_manifest)
			USBDFU_SwitchToApp();
#endif

			// The device enters the Default state
			deviceState = USBD_STATE_DEFAULT;
			//      MCK + UDPCK are already enabled
			//      Pull-Up is already connected
			//      Transceiver must be enabled
			//      Endpoint 0 must be enabled

			UDPHS_ResetEndpoints();
			UDPHS_DisableEndpoints();
			usbdhs_configureEndpoint(0);

			// Flush and enable the Suspend interrupt
			AT91C_BASE_UDPHS->UDPHS_CLRINT = AT91C_UDPHS_WAKE_UP | AT91C_UDPHS_DET_SUSPD;

			//// Enable the Start Of Frame (SOF) interrupt if needed
			//if (pCallbacks->startOfFrame != 0)
			//{
			//    AT91C_BASE_UDPHS->UDPHS_IEN |= AT91C_UDPHS_IEN_SOF;
			//}

			// Invoke the Reset callback
			//USBDCallbacks_Reset();

			// Acknowledge end of bus reset interrupt
			AT91C_BASE_UDPHS->UDPHS_CLRINT = AT91C_UDPHS_ENDRESET;

			AT91C_BASE_UDPHS->UDPHS_IEN |= AT91C_UDPHS_DET_SUSPD;
		}
		// Handle upstream resume interrupt
		else if(status & AT91C_UDPHS_UPSTR_RES) {

			// - Acknowledge the IT
			AT91C_BASE_UDPHS->UDPHS_CLRINT = AT91C_UDPHS_UPSTR_RES;
		}
		// Endpoint interrupts
		else {
#ifndef DMA
			// Handle endpoint interrupts
			for(numIT = 0; numIT < NUM_IT_MAX; numIT++) {

				if((status & (1 << SHIFT_INTERUPT << numIT)) != 0) {

					UDPHS_EndpointHandler(numIT);
				}
			}
#else
			// Handle endpoint control interrupt
			if((status & (1 << SHIFT_INTERUPT << 0)) != 0) {

				UDPHS_EndpointHandler(0);
			} else {

				numIT = 1;
				while((status & (0x7E << SHIFT_DMA)) != 0) {

					// Check if endpoint has a pending interrupt
					if((status & (1 << SHIFT_DMA << numIT)) != 0) {

						UDPHS_DmaHandler(numIT);
						status &= ~(1 << SHIFT_DMA << numIT);
						if(status != 0) {

						}
					}
					numIT++;
				}
			}
#endif
		}
		// Retrieve new interrupt status
		status = AT91C_BASE_UDPHS->UDPHS_INTSTA;
		status &= AT91C_BASE_UDPHS->UDPHS_IEN;

		//TRACE_DEBUG_WP("\n\r");
		if(status != 0) {

			//TRACE_DEBUG_WP("  - ");
		}
	}

	if(deviceState >= USBD_STATE_POWERED) {
		//LED_Clear(USBD_LEDUSB);
	}
}

static inline void udphs_EnablePeripheral(void)
{
	AT91C_BASE_PMC->PMC_PCER = (1 << AT91C_ID_UDPHS);
}

void usbdhs_configure(void)
{
	unsigned char i;

	// Reset endpoint structures
	UDPHS_ResetEndpoints();

	// Enables the UDPHS peripheral
	udphs_EnablePeripheral();

	// XCHQ[2010.1.21]: From IP recomendation, the order for init is:
	//  - reset IP (EN_UDPHS), - enable PLL 480M, - enable USB Clock

	// Enables the USB Clock
	// UDPHS_EnableUsbClock();

	// Configure the pull-up on D+ and disconnect it
	AT91C_BASE_UDPHS->UDPHS_CTRL |= AT91C_UDPHS_DETACH; // detach
	AT91C_BASE_UDPHS->UDPHS_CTRL |= AT91C_UDPHS_PULLD_DIS; // Disable Pull Down

	// Reset and enable IP UDPHS
	AT91C_BASE_UDPHS->UDPHS_CTRL &= ~(unsigned int)AT91C_UDPHS_EN_UDPHS;
	AT91C_BASE_UDPHS->UDPHS_CTRL |= AT91C_UDPHS_EN_UDPHS;
	// Enable and disable of the transceiver is automaticaly done by the IP.

	// Enables the USB Clock
	// (XCHQ[2010.1.21], IP recomendation, setup clock after reset IP)
	UDPHS_EnableUsbClock();

	// With OR without DMA !!!
	// Initialization of DMA
	for(i = 1; i <= ((AT91C_BASE_UDPHS->UDPHS_IPFEATURES & AT91C_UDPHS_DMA_CHANNEL_NBR) >> 4); i++) {

		// RESET endpoint canal DMA:
		// DMA stop channel command
		AT91C_BASE_UDPHS->UDPHS_DMA[i].UDPHS_DMACONTROL = 0; // STOP command

		// Disable endpoint
		AT91C_BASE_UDPHS->UDPHS_EPT[i].UDPHS_EPTCTLDIS = (uint)
			AT91C_UDPHS_SHRT_PCKT |
			AT91C_UDPHS_BUSY_BANK |
			AT91C_UDPHS_NAK_OUT |
			AT91C_UDPHS_NAK_IN |
			AT91C_UDPHS_STALL_SNT |
			AT91C_UDPHS_RX_SETUP |
			AT91C_UDPHS_TX_PK_RDY |
			AT91C_UDPHS_TX_COMPLT |
			AT91C_UDPHS_RX_BK_RDY |
			AT91C_UDPHS_ERR_OVFLW |
			AT91C_UDPHS_MDATA_RX |
			AT91C_UDPHS_DATAX_RX |
			AT91C_UDPHS_NYET_DIS |
			AT91C_UDPHS_INTDIS_DMA |
			AT91C_UDPHS_AUTO_VALID |
			AT91C_UDPHS_EPT_DISABL;

		// Clear status endpoint
		AT91C_BASE_UDPHS->UDPHS_EPT[i].UDPHS_EPTCLRSTA =
			AT91C_UDPHS_TOGGLESQ |
			AT91C_UDPHS_FRCESTALL |
			AT91C_UDPHS_RX_BK_RDY |
			AT91C_UDPHS_TX_COMPLT |
			AT91C_UDPHS_RX_SETUP |
			AT91C_UDPHS_STALL_SNT |
			AT91C_UDPHS_NAK_IN |
			AT91C_UDPHS_NAK_OUT;

		// Reset endpoint config
		AT91C_BASE_UDPHS->UDPHS_EPT[i].UDPHS_EPTCTLENB = 0;

		// Reset DMA channel (Buff count and Control field)
		AT91C_BASE_UDPHS->UDPHS_DMA[i].UDPHS_DMACONTROL = AT91C_UDPHS_LDNXT_DSC; // NON STOP command

		// Reset DMA channel 0 (STOP)
		AT91C_BASE_UDPHS->UDPHS_DMA[i].UDPHS_DMACONTROL = 0; // STOP command

		// Clear DMA channel status (read the register for clear it)
		AT91C_BASE_UDPHS->UDPHS_DMA[i].UDPHS_DMASTATUS = AT91C_BASE_UDPHS->UDPHS_DMA[i].UDPHS_DMASTATUS;

	}

	AT91C_BASE_UDPHS->UDPHS_TST = 0;
	AT91C_BASE_UDPHS->UDPHS_IEN = 0;
	AT91C_BASE_UDPHS->UDPHS_CLRINT = AT91C_UDPHS_UPSTR_RES | AT91C_UDPHS_ENDOFRSM | AT91C_UDPHS_WAKE_UP | AT91C_UDPHS_ENDRESET | AT91C_UDPHS_IEN_SOF
	    | AT91C_UDPHS_MICRO_SOF | AT91C_UDPHS_DET_SUSPD;

	// Device is in the Attached state
	deviceState = USBD_STATE_SUSPENDED;
	previousDeviceState = USBD_STATE_POWERED;

	// Enable interrupts
	AT91C_BASE_UDPHS->UDPHS_IEN = AT91C_UDPHS_ENDOFRSM | AT91C_UDPHS_WAKE_UP | AT91C_UDPHS_DET_SUSPD;

	// Disable USB clocks
	UDPHS_DisableUsbClock();

	// Configure interrupts
	irq_configure(AT91C_ID_UDPHS, 0);
	irq_enable(AT91C_ID_UDPHS);
}

void usbdhs_connect(void)
{
	AT91C_BASE_UDPHS->UDPHS_CTRL &= ~(unsigned int)AT91C_UDPHS_DETACH; // Pull Up on DP
	AT91C_BASE_UDPHS->UDPHS_CTRL |= AT91C_UDPHS_PULLD_DIS; // Disable Pull Down
}

void usbdhs_setConfiguration(uint cfgnum)
{
    // Check the request
    if( cfgnum != 0 ) {
        // Enter Configured state
        deviceState = USBD_STATE_CONFIGURED;
    }
    // If the configuration number is zero, the device goes back to the Address
    // state
    else  {
        // Go back to Address state
        deviceState = USBD_STATE_ADDRESS;

        // Abort all transfers
        UDPHS_DisableEndpoints();
    }
}

char usbdhs_write(unsigned char bEndpoint, const void *pData, unsigned int dLength, TransferCallback fCallback, void *pArgument)
{
	Endpoint *pEndpoint = &(endpoints[bEndpoint]);
	Transfer *pTransfer = (Transfer*)&(pEndpoint->transfer);

	// Return if the endpoint is not in IDLE state
	if(pEndpoint->state != UDP_ENDPOINT_IDLE) {

		return USBD_STATUS_LOCKED;
	}

	pEndpoint->sendZLP = 0;
	// Setup the transfer descriptor
	pTransfer->pData = (void *)pData;
	pTransfer->remaining = dLength;
	pTransfer->buffered = 0;
	pTransfer->transferred = 0;
	pTransfer->fCallback = fCallback;
	pTransfer->pArgument = pArgument;

	// Send one packet
	pEndpoint->state = UDP_ENDPOINT_SENDING;

#ifdef DMA
	// Test if endpoint type control
	if(AT91C_UDPHS_EPT_TYPE_CTL_EPT == (AT91C_UDPHS_EPT_TYPE & (AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCFG))) {
#endif
		// Enable endpoint IT
		AT91C_BASE_UDPHS->UDPHS_IEN |= (1 << SHIFT_INTERUPT << bEndpoint);
		AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCTLENB = AT91C_UDPHS_TX_PK_RDY;

#ifdef DMA
	} else {
		if(pTransfer->remaining == 0) {
			// DMA not handle ZLP
			AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTSETSTA = AT91C_UDPHS_TX_PK_RDY;
			// Enable endpoint IT
			AT91C_BASE_UDPHS->UDPHS_IEN |= (1 << SHIFT_INTERUPT << bEndpoint);
			AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCTLENB = AT91C_UDPHS_TX_PK_RDY;
		} else {
			// Others endpoints (not control)
			if(pTransfer->remaining > DMA_MAX_FIFO_SIZE) {

				// Transfer the max
				pTransfer->buffered = DMA_MAX_FIFO_SIZE;
			} else {
				// Transfer the good size
				pTransfer->buffered = pTransfer->remaining;
			}

			AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMAADDRESS = (unsigned int)(pTransfer->pData);

			// Clear unwanted interrupts
			AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMASTATUS;
			// Enable DMA endpoint interrupt
			AT91C_BASE_UDPHS->UDPHS_IEN |= (1 << SHIFT_DMA << bEndpoint);
			// DMA config
			AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMACONTROL = 0; // raz
			AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMACONTROL = (((pTransfer->buffered << 16) & AT91C_UDPHS_BUFF_COUNT) | AT91C_UDPHS_END_B_EN
			    | AT91C_UDPHS_END_BUFFIT | AT91C_UDPHS_CHANN_ENB);
		}
	}
#endif

	return USBD_STATUS_SUCCESS;
}

char usbdhs_read(unsigned char bEndpoint, void *pData, unsigned int dLength, TransferCallback fCallback, void *pArgument)
{
	Endpoint *pEndpoint = &(endpoints[bEndpoint]);
	Transfer *pTransfer = (Transfer*)&(pEndpoint->transfer);

	// Return if the endpoint is not in IDLE state
	if(pEndpoint->state != UDP_ENDPOINT_IDLE) {

		return USBD_STATUS_LOCKED;
	}

	// Endpoint enters Receiving state
	pEndpoint->state = UDP_ENDPOINT_RECEIVING;

	// Set the transfer descriptor
	pTransfer->pData = pData;
	pTransfer->remaining = dLength;
	pTransfer->buffered = 0;
	pTransfer->transferred = 0;
	pTransfer->fCallback = fCallback;
	pTransfer->pArgument = pArgument;

#ifdef DMA
	// Test if endpoint type control
	if(AT91C_UDPHS_EPT_TYPE_CTL_EPT == (AT91C_UDPHS_EPT_TYPE & (AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCFG))) {
#endif
		// Control endpoint
		// Enable endpoint IT
		AT91C_BASE_UDPHS->UDPHS_IEN |= (1 << SHIFT_INTERUPT << bEndpoint);
		AT91C_BASE_UDPHS->UDPHS_EPT[bEndpoint].UDPHS_EPTCTLENB = AT91C_UDPHS_RX_BK_RDY;
#ifdef DMA
	} else {
		// Others endpoints (not control)
		if(pTransfer->remaining > DMA_MAX_FIFO_SIZE) {

			// Transfer the max
			pTransfer->buffered = DMA_MAX_FIFO_SIZE;
		} else {
			// Transfer the good size
			pTransfer->buffered = pTransfer->remaining;
		}

		AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMAADDRESS = (unsigned int)(pTransfer->pData);

		// Clear unwanted interrupts
		AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMASTATUS;

		// Enable DMA endpoint interrupt
		AT91C_BASE_UDPHS->UDPHS_IEN |= (1 << SHIFT_DMA << bEndpoint);

		// DMA config
		AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMACONTROL = 0; // raz
		AT91C_BASE_UDPHS->UDPHS_DMA[bEndpoint].UDPHS_DMACONTROL = (((pTransfer->buffered << 16) & AT91C_UDPHS_BUFF_COUNT) | AT91C_UDPHS_END_TR_EN
		    | AT91C_UDPHS_END_TR_IT | AT91C_UDPHS_END_B_EN | AT91C_UDPHS_END_BUFFIT | AT91C_UDPHS_CHANN_ENB);
	}
#endif

	return USBD_STATUS_SUCCESS;
}

unsigned char usbdhs_stall(uint endpoint)
{
	Endpoint* ep = &(endpoints[endpoint]);

	// Check that endpoint is in Idle state
	if(ep->state != UDP_ENDPOINT_IDLE)
		return USBD_STATUS_LOCKED;

	AT91C_BASE_UDPHS->UDPHS_EPT[endpoint].UDPHS_EPTSETSTA = AT91C_UDPHS_FRCESTALL;

	return USBD_STATUS_SUCCESS;
}

Bool usbdhs_isHighSpeed(void)
{
	if(AT91C_UDPHS_SPEED == (AT91C_BASE_UDPHS->UDPHS_INTSTA & AT91C_UDPHS_SPEED))
		return True;
	else return False;
}

Bool usbdhs_isHalted(uint endpoint)
{
	Endpoint* ep = &(endpoints[endpoint]);

	if(ep->state == UDP_ENDPOINT_HALTED)
		return True;
	else return False;
}

void usbdhs_setAddress(uint address)
{
	// set address
	AT91C_BASE_UDPHS->UDPHS_CTRL &= ~(uint)(AT91C_UDPHS_DEV_ADDR & 0x7f); // RAZ Address
	AT91C_BASE_UDPHS->UDPHS_CTRL |= address | AT91C_UDPHS_FADDR_EN;

	if(address == 0x00) {
		// if the address is 0, the device returns to the Default state
		deviceState = USBD_STATE_DEFAULT;
	} else {
		// if the address is non-zero, the device enters the Address state
		deviceState = USBD_STATE_ADDRESS;
	}
}
