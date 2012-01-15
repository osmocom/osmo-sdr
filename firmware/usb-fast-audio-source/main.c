/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2009, Atmel Corporation
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
/**
 *  \file
 *
 *  This file contains all the specific code for the
 *  usb_fast_source example.
 */

/*----------------------------------------------------------------------------
 *         Headers
 *----------------------------------------------------------------------------*/

#include <board.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdbool.h>

#include <irq/irq.h>
#include <pio/pio.h>
#include <pio/pio_it.h>
#include <utility/trace.h>
#include <utility/led.h>

#include <usb/device/core/USBD.h>

#include <usb/device/dfu/dfu.h>

#include <fast_source.h>

/*----------------------------------------------------------------------------
 *         Definitions
 *----------------------------------------------------------------------------*/

#if 0
/**  Number of available audio buffers. */
#define BUFFER_NUMBER       8
/**  Size of one buffer in bytes. */
#define BUFFER_SIZE         (AUDDLoopRecDriver_BYTESPERFRAME*2)
#endif

/// Use for power management
#define STATE_IDLE    0
/// The USB device is in suspend state
#define STATE_SUSPEND 4
/// The USB device is in resume state
#define STATE_RESUME  5

//------------------------------------------------------------------------------
//         Internal variables
//------------------------------------------------------------------------------
/// State of USB, for suspend and resume
unsigned char USBState = STATE_IDLE;

#if 0
/**  Data buffers for receiving audio frames from the USB host. */
static uint8_t buffers[BUFFER_NUMBER][BUFFER_SIZE];
/**  Number of samples stored in each data buffer. */
static uint32_t bufferSizes[BUFFER_NUMBER];
/**  Next buffer in which USB data can be stored. */
static uint32_t inBufferIndex = 0;
/**  Number of buffers that can be sent to the DAC. */
static volatile uint32_t numBuffersToSend = 0;
#endif

/**  Current state of the playback stream interface. */
static volatile uint8_t isPlyActive = 0;
/**  Current state of the record stream interface. */
static volatile uint8_t isRecActive = 0;

/*----------------------------------------------------------------------------
 *         VBus monitoring (optional)
 *----------------------------------------------------------------------------*/

/**  VBus pin instance. */
static const Pin pinVbus = PIN_USB_VBUS;
static const Pin pinPB = PIN_PUSHBUTTON_1;

/**
 *  Handles interrupts coming from PIO controllers.
 */
static void ISR_Vbus(const Pin *pPin)
{
    /* Check current level on VBus */
    if (PIO_Get(&pinVbus))
    {
        TRACE_INFO("VBUS conn\n\r");
        USBD_Connect();
    }
    else
    {
        TRACE_INFO("VBUS discon\n\r");
        USBD_Disconnect();
    }
}

/**
 *  Configures the VBus pin to trigger an interrupt when the level on that pin
 *  changes.
 */
static void VBus_Configure( void )
{
    /* Configure PIO */
    PIO_Configure(&pinVbus, 1);
    PIO_ConfigureIt(&pinVbus, ISR_Vbus);
    PIO_EnableIt(&pinVbus);

    /* Check current level on VBus */
    if (PIO_Get(&pinVbus))
    {
        /* if VBUS present, force the connect */
        USBD_Connect();
    }
    else
    {
        TRACE_INFO("discon\n\r");
        USBD_Disconnect();
    }
}

/*----------------------------------------------------------------------------
 *         USB Power Control
 *----------------------------------------------------------------------------*/

#ifdef PIN_USB_POWER_ENA
/** Power Enable A (MicroAB Socket) pin instance. */
static const Pin pinPOnA = PIN_USB_POWER_ENA;
#endif
#ifdef PIN_USB_POWER_ENB
/** Power Enable B (A Socket) pin instance. */
static const Pin pinPOnB = PIN_USB_POWER_ENB;
#endif
#ifdef PIN_USB_POWER_ENC
/** Power Enable C (A Socket) pin instance. */
static const Pin pinPOnC = PIN_USB_POWER_ENC;
#endif
/**
 * Configures the Power Enable pin to disable self power.
 */
static void USBPower_Configure( void )
{
  #ifdef PIN_USB_POWER_ENA
    PIO_Configure(&pinPOnA, 1);
  #endif
  #ifdef PIN_USB_POWER_ENB
    PIO_Configure(&pinPOnB, 1);
  #endif
  #ifdef PIN_USB_POWER_ENC
    PIO_Configure(&pinPOnC, 1);
  #endif
}

/*----------------------------------------------------------------------------
 *         Internal functions
 *----------------------------------------------------------------------------*/

#if 0
/**
 *  Invoked when a frame has been received.
 */
static void FrameReceived(uint32_t unused,
                          uint8_t status,
                          uint32_t transferred,
                          uint32_t remaining)
{
    if (status == USBD_STATUS_SUCCESS)
    {
        /* Loopback! add this buffer to write list */
        if (!isRecActive)  {}
        else
        {
            AUDDLoopRecDriver_Write(buffers[inBufferIndex],
                                         AUDDLoopRecDriver_BYTESPERFRAME,
					 NULL, 0);
        }

        /* Update input status data */
        bufferSizes[inBufferIndex] = transferred
                                        / AUDDLoopRecDriver_BYTESPERSAMPLE;
        inBufferIndex = (inBufferIndex + 1) % BUFFER_NUMBER;
        numBuffersToSend++;

    }
    else if (status == USBD_STATUS_ABORTED)
    {
        /* Error , ABORT, add NULL buffer */
        bufferSizes[inBufferIndex] = 0;
        inBufferIndex = (inBufferIndex + 1) % BUFFER_NUMBER;
        numBuffersToSend++;
    }
    else
    {
        /* Packet is discarded */
    }

    /* Receive next packet */
    AUDDLoopRecDriver_Read(buffers[inBufferIndex],
                           AUDDLoopRecDriver_BYTESPERFRAME,
                           (TransferCallback) FrameReceived,
                           0); // No optional argument
}
#endif

/*----------------------------------------------------------------------------
 *         Callbacks re-implementation
 *----------------------------------------------------------------------------*/

//------------------------------------------------------------------------------
/// Invoked when the USB device leaves the Suspended state. By default,
/// configures the LEDs.
//------------------------------------------------------------------------------
void USBDCallbacks_Resumed(void)
{
    // Initialize LEDs
    LED_Configure(USBD_LEDPOWER);
    LED_Set(USBD_LEDPOWER);
    LED_Configure(USBD_LEDUSB);
    LED_Clear(USBD_LEDUSB);
    USBState = STATE_RESUME;
}

//------------------------------------------------------------------------------
/// Invoked when the USB device gets suspended. By default, turns off all LEDs.
//------------------------------------------------------------------------------
void USBDCallbacks_Suspended(void)
{
    // Turn off LEDs
    LED_Clear(USBD_LEDPOWER);
    LED_Clear(USBD_LEDUSB);
    USBState = STATE_SUSPEND;
}



/*----------------------------------------------------------------------------
 *         Exported functions
 *----------------------------------------------------------------------------*/

extern void USBD_IrqHandler(void);
/**
 *  \brief usb_audio_looprec Application entry point.
 *
 *  Starts the driver and waits for an audio input stream to forward to the DAC.
 */
int main(void)
{
    volatile uint8_t usbConn = 0;
    volatile uint8_t plyOn = 0, recOn = 0;

    TRACE_CONFIGURE(DBGU_STANDARD, 115200, BOARD_MCK);

    printf("-- USB Device Fast Audio Source %s --\n\r", SOFTPACK_VERSION);
    printf("-- %s\n\r", BOARD_NAME);
    printf("-- Compiled: %s %s --\n\r", __DATE__, __TIME__);

    /* If they are present, configure Vbus & Wake-up pins */
    PIO_InitializeInterrupts(0);

    /* Initialize all USB power (off) */
    USBPower_Configure();

    /* Audio STREAM LED */
    LED_Configure(USBD_LEDOTHER);

    /* USB audio driver initialization */
    fastsource_init();

    /* connect if needed */
    VBus_Configure();

    PIO_Configure(&pinPB, 1);

    static int state = 0;

    /* Infinite loop */
    while (1)
    {
        if (USBD_GetState() < USBD_STATE_CONFIGURED)
        {
            usbConn = 0;
            continue;
        }
	if (state == 0) {
    		fastsource_start();
		state = 1;
	}
        if (plyOn)
        {
            if (isPlyActive == 0)
            {
                printf("plyE ");
                plyOn = 0;
            }
        }
        else if (isPlyActive)
        {
#if 0
            /* Try to Start Reading the incoming audio stream */
            AUDDLoopRecDriver_Read(buffers[inBufferIndex],
                                   AUDDLoopRecDriver_BYTESPERFRAME,
                                   (TransferCallback) FrameReceived,
                                   0); // No optional argument

            printf("plyS ");
            plyOn = 1;
#endif
        }
        if (recOn)
        {
            if (isRecActive == 0)
            {
                printf("recE ");
                recOn = 0;
            }
        }
        else if (isRecActive)
        {
            printf("recS ");
            recOn = 1;
        }

#ifdef dfu
	if (PIO_Get(&pinPB) == 0)
		DFURT_SwitchToDFU();
#endif
    }
}
/** \endcond */
