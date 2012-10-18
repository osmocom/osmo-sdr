#ifndef INCLUDE_DBGIO_H
#define INCLUDE_DBGIO_H

#include "../crt/types.h"

void dbgio_configure(uint baudrate);
int dbgio_getChar(void);
void dbgio_putChar(u8 c);
void dbgio_putCharDirect(u8 c);
Bool dbgio_isRxReady(void);
void dbgio_flushOutput(void);

#endif // INCLUDE_DBGIO_H
