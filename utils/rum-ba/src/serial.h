#ifndef INCLUDE_SERIAL_H
#define INCLUDE_SERIAL_H

#include <stdint.h>

int serialOpen(const char* device);
void serialClose(int fd);
int  serialPutC(int fd, char c);
int serialPutS(int fd, const char* str);
int serialGetC(int fd, char* c, int timeout);
int serialGetS(int fd, char* str, int len, int timeout);
int serialExpect(int fd, const char* str, int timeout);

#endif // INCLUDE_SERIAL_H
