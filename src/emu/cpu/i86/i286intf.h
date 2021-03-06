/* ASG 971222 -- rewrote this interface */
#ifndef __I286INTR_H_
#define __I286INTR_H_

#include "i86intf.h"

enum
{
	I80286_PC=0,
	I80286_IP,
	I80286_AX,
	I80286_CX,
	I80286_DX,
	I80286_BX,
	I80286_SP,
	I80286_BP,
	I80286_SI,
	I80286_DI,
	I80286_FLAGS,
	I80286_ES,
	I80286_CS,
	I80286_SS,
	I80286_DS,
	I80286_ES_2,
	I80286_CS_2,
	I80286_SS_2,
	I80286_DS_2,
	I80286_MSW,
	I80286_GDTR_BASE,
	I80286_IDTR_BASE,
	I80286_LDTR_BASE,
	I80286_TR_BASE,
	I80286_GDTR_LIMIT,
	I80286_IDTR_LIMIT,
	I80286_LDTR_LIMIT,
	I80286_TR_LIMIT,
	I80286_VECTOR,
	I80286_PENDING,
	I80286_NMI_STATE,
	I80286_IRQ_STATE
};

/* Public functions */
extern void i80286_set_irq_line(int irqline, int state);
extern void i80286_set_irq_callback(int (*callback)(int irqline));
extern const char *i80286_info(void *context, int regnum);
void i80286_get_info(UINT32 state, cpuinfo *info);

#endif
