#ifndef INCLUDE_TWI_H
#define INCLUDE_TWI_H

#include "../crt/types.h"

void twi_configure(void);

typedef struct {
	int(*probe)(u8 address);
	int(*read)(u8 address, u32 iaddress, uint isize, u8* data, uint num);
	int(*write)(u8 address, u32 iaddress, uint isize, const u8* data, uint num);
} TWI;

extern const TWI g_twi0;
extern const TWI g_twi1;

#endif // INCLUDE_TWI_H
