#ifndef INCLUDE_VECTORS_H
#define INCLUDE_VECTORS_H

typedef void(*InterruptHandler)(void);
extern InterruptHandler interrupt_table[];

void resetHandler(void); // implemented in startup.c
void dbgio_irqHandler(void); // implemented in dbgio.c
void usbd_irqHandler(void); // implemented in usbdhs.c
void ssc0_irqHandler(void); // implemented in sdrssc.c
void mci0_irqHandler(void); // implemented in sdrmci.c
void hdma_irqHandler(void); // implemented in sdrssc.c or sdrmci.c

#endif // INCLUDE_VECTORS_H
