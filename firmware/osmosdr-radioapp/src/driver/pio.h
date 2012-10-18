#ifndef INCLUDE_PIO_H
#define INCLUDE_PIO_H

#include "../at91sam3u4/AT91SAM3U4.h"
#include "../crt/types.h"

/// The pin is controlled by the associated signal of peripheral A.
#define PIO_PERIPH_A                0
/// The pin is controlled by the associated signal of peripheral B.
#define PIO_PERIPH_B                1
/// The pin is an input.
#define PIO_INPUT                   2
/// The pin is an output and has a default level of 0.
#define PIO_OUTPUT_0                3
/// The pin is an output and has a default level of 1.
#define PIO_OUTPUT_1                4

/// Default pin configuration (no attribute).
#define PIO_DEFAULT                 (0 << 0)
/// The internal pin pull-up is active.
#define PIO_PULLUP                  (1 << 0)
/// The internal glitch filter is active.
#define PIO_DEGLITCH                (1 << 1)
/// The pin is open-drain.
#define PIO_OPENDRAIN               (1 << 2)

typedef struct {
	u32 mask;
	AT91S_PIO *pio;
	u8 id;
	u8 type;
	u8 attribute;
} PinConfig;

void pio_configure(const PinConfig* list, uint size);
void pio_set(const PinConfig* pin);
void pio_clear(const PinConfig* pin);
uint pio_get(const PinConfig* pin);

#endif // INCLUDE_PIO_H
