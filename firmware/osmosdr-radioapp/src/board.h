#ifndef INCLUDE_BOARD_H
#define INCLUDE_BOARD_H

#include "at91sam3u4/AT91SAM3U4.h"

// frequency for the MCU crystal
#define BOARD_CRYSTAL 12000000

// master clock we want to use
//#define BOARD_MCK 48000000
#define BOARD_MCK 96000000

typedef enum IRQn {
	/******  Cortex-M3 Processor Exceptions Numbers ***************************************************/
	NonMaskableInt_IRQn = -14, /*!< 2 Non Maskable Interrupt */
	MemoryManagement_IRQn = -12, /*!< 4 Cortex-M3 Memory Management Interrupt */
	BusFault_IRQn = -11, /*!< 5 Cortex-M3 Bus Fault Interrupt */
	UsageFault_IRQn = -10, /*!< 6 Cortex-M3 Usage Fault Interrupt */
	SVCall_IRQn = -5, /*!< 11 Cortex-M3 SV Call Interrupt */
	DebugMonitor_IRQn = -4, /*!< 12 Cortex-M3 Debug Monitor Interrupt */
	PendSV_IRQn = -2, /*!< 14 Cortex-M3 Pend SV Interrupt */
	SysTick_IRQn = -1, /*!< 15 Cortex-M3 System Tick Interrupt */

	/******  AT91SAM3U4 specific Interrupt Numbers *********************************************************/
	IROn_SUPC = AT91C_ID_SUPC, // SUPPLY CONTROLLER
	IROn_RSTC = AT91C_ID_RSTC, // RESET CONTROLLER
	IROn_RTC = AT91C_ID_RTC, // REAL TIME CLOCK
	IROn_RTT = AT91C_ID_RTT, // REAL TIME TIMER
	IROn_WDG = AT91C_ID_WDG, // WATCHDOG TIMER
	IROn_PMC = AT91C_ID_PMC, // PMC
	IROn_EFC0 = AT91C_ID_EFC0, // EFC0
	IROn_EFC1 = AT91C_ID_EFC1, // EFC1
	IROn_DBGU = AT91C_ID_DBGU, // DBGU
	IROn_HSMC4 = AT91C_ID_HSMC4, // HSMC4
	IROn_PIOA = AT91C_ID_PIOA, // Parallel IO Controller A
	IROn_PIOB = AT91C_ID_PIOB, // Parallel IO Controller B
	IROn_PIOC = AT91C_ID_PIOC, // Parallel IO Controller C
	IROn_US0 = AT91C_ID_US0, // USART 0
	IROn_US1 = AT91C_ID_US1, // USART 1
	IROn_US2 = AT91C_ID_US2, // USART 2
	IROn_US3 = AT91C_ID_US3, // USART 3
	IROn_MCI0 = AT91C_ID_MCI0, // Multimedia Card Interface
	IROn_TWI0 = AT91C_ID_TWI0, // TWI 0
	IROn_TWI1 = AT91C_ID_TWI1, // TWI 1
	IROn_SPI0 = AT91C_ID_SPI0, // Serial Peripheral Interface
	IROn_SSC0 = AT91C_ID_SSC0, // Serial Synchronous Controller 0
	IROn_TC0 = AT91C_ID_TC0, // Timer Counter 0
	IROn_TC1 = AT91C_ID_TC1, // Timer Counter 1
	IROn_TC2 = AT91C_ID_TC2, // Timer Counter 2
	IROn_PWMC = AT91C_ID_PWMC, // Pulse Width Modulation Controller
	IROn_ADCC0 = AT91C_ID_ADC12B, // ADC controller0
	IROn_ADCC1 = AT91C_ID_ADC, // ADC controller1
	IROn_HDMA = AT91C_ID_HDMA, // HDMA
	IROn_UDPHS = AT91C_ID_UDPHS // USB Device High Speed
} IRQn_Type;

#define CHIP_USB_NUMENDPOINTS 7

#define CHIP_USB_ENDPOINTS_MAXPACKETSIZE(i) \
	((i == 0) ? 64 : \
	((i == 1) ? 512 : \
	((i == 2) ? 512 : \
	((i == 3) ? 64 : \
	((i == 4) ? 64 : \
	((i == 5) ? 1024 : \
	((i == 6) ? 1024 : 0 )))))))

#define CHIP_USB_ENDPOINTS_BANKS(i) \
	((i == 0) ? 1 : \
	((i == 1) ? 2 : \
	((i == 2) ? 2 : \
	((i == 3) ? 3 : \
	((i == 4) ? 3 : \
	((i == 5) ? 3 : \
	((i == 6) ? 3 : 0 )))))))

#define CHIP_USB_ENDPOINTS_DMA(i) \
	((i == 1) ? 1 : \
	((i == 2) ? 1 : \
	((i == 3) ? 1 : \
	((i == 4) ? 1 : \
	((i == 5) ? 1 : \
	((i == 6) ? 1 : 0 ))))))

#define BOARD_USB_VENDOR 0x16c0
#define BOARD_USB_PRODUCT 0x0763
#define BOARD_USB_RELEASE 0x0100

#define BOARD_USB_BMATTRIBUTES USBConfigurationDescriptor_SELFPOWERED_RWAKEUP

#define BOARD_USB_NUMINTERFACES 1

#define BOARD_OSDR_SPI AT91C_BASE_SPI0
#define BOARD_OSDR_FPGA_SPI_NPCS 0

#define BOARD_SSC_DMA_CHANNEL 2
#define BOARD_SSC_DMA_HW_SRC_REQ_ID AT91C_HDMA_SRC_PER_4
#define BOARD_SSC_DMA_HW_DEST_REQ_ID AT91C_HDMA_DST_PER_3

#define BOARD_MCI_DMA_CHANNEL 0
#define BOARD_MCI_DMA_HW_SRC_REQ_ID AT91C_HDMA_SRC_PER_0
#define BOARD_MCI_DMA_HW_DEST_REQ_ID AT91C_HDMA_DST_PER_0

//#define BOARD_SAMPLE_SOURCE_SSC 1
//#undef BOARD_SAMPLE_SOURCE_MCI

#undef BOARD_SAMPLE_SOURCE_SSC
#define BOARD_SAMPLE_SOURCE_MCI 1

#if defined(BOARD_SAMPLE_SOURCE_SSC) && defined(BOARD_SAMPLE_SOURCE_MCI)
#error "Cannot use SSC and MCI as sample source at the same time!"
#endif
#if (!defined(BOARD_SAMPLE_SOURCE_SSC)) && (!defined(BOARD_SAMPLE_SOURCE_MCI))
#error "Select SSC or MCI as sample source!"
#endif

#endif // INCLUDE_BOARD_H
