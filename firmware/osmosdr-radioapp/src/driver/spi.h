#ifndef INCLUDE_SPI_H
#define INCLUDE_SPI_H

#include "../at91sam3u4/AT91SAM3U4.h"
#include "../crt/types.h"

#define SPI_SCBR(baudrate, masterClock) ((uint) (masterClock / baudrate) << 8)
#define SPI_DLYBS(delay, masterClock) ((uint) (((masterClock / 1000000) * delay) / 1000) << 16)
#define SPI_DLYBCT(delay, masterClock) ((uint) (((masterClock / 1000000) * delay) / 32000) << 24)
#define SPI_PCS(npcs) ((~(1 << npcs) & 0xf) << 16)

void spi_configure(AT91S_SPI* spi, uint id, uint configuration);
void spi_configureNPCS(AT91S_SPI* spi, uint npcs, uint configuration);
void spi_enable(AT91S_SPI* spi);
void spi_disable(AT91S_SPI* spi);
void spi_write(AT91S_SPI* spi, uint npcs, u16 data);
u16 spi_read(AT91S_SPI* spi);
Bool spi_isFinished(AT91S_SPI* spi);

#endif // INCLUDE_SPI_H
