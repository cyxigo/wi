#ifndef WI_DISASM_H
#define WI_DISASM_H

#include "wi_box.h"

int
wi_prototype_disasm_instr(wi_prototype_t* prototype, int offset);
void
wi_prototype_disasm(wi_prototype_t* prototype);

#endif
