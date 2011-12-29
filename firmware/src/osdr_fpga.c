
#include <pio/pio.h>

static const Pin fon_pin = PIN_FON;

void osdr_fpga_power(int on)
{
	if (on)
		PIO_Set(&fon_pin);
	else
		PIO_Clear(&fon_pin);
}
