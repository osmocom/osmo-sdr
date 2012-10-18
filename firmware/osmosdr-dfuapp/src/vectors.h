#ifndef INCLUDE_VECTORS_H
#define INCLUDE_VECTORS_H

typedef void(*InterruptHandler)(void);
extern InterruptHandler interrupt_table[];

void resetHandler(void); // implemented in startup.c
void dbgio_irqHandler(void); // implemented in dbgio.c
void usbd_irqHandler(void); // implemented in usbdhs.c

#endif // INCLUDE_VECTORS_H
