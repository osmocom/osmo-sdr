#ifndef INCLUDE_SERIAL_H
#define INCLUDE_SERIAL_H

#include "utils.h"

HANDLE serialOpen(const char* dev);
void serialClose(HANDLE fd);
int serialSetBaudrate(HANDLE fd, int baudrate);
int serialPutC(HANDLE fd, uint8_t c);
int serialGetC(HANDLE fd, uint8_t* c, int timeout);

int serialPutS(HANDLE fd, const char* str);
int serialGetS(HANDLE fd, char* str, int len, int timeout);
int serialExpect(HANDLE fd, const char* str, int timeout);

#endif // INCLUDE_SERIAL_H
