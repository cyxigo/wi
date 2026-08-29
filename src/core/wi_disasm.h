#ifndef WI_DISASM_H
#define WI_DISASM_H

#include "wi_box.h"
#include "wi_state.h"

int
wi_prototype_disasm_instr(struct wi_state* state, struct wi_prototype* prototype, int offset);
void
wi_prototype_disasm(struct wi_state* state, struct wi_prototype* prototype);

#endif
