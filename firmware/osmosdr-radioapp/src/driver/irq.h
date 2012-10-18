#ifndef INCLUDE_IRQ_H
#define INCLUDE_IRQ_H

#include "../crt/types.h"

void irq_configure(uint source, uint priority);
void irq_enable(uint source);
void irq_disable(uint source);

#endif // INCLUDE_IRQ_H
