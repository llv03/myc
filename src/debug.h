#ifndef MYC_DEBUG_H
#define MYC_DEBUG_H

#include "chunk.h"
#include "compiler.h"

void disassemble_chunk(Chunk *chunk, const char *name);
int  disassemble_instruction(Chunk *chunk, int offset);
void disassemble_program(CompileResult *prog);

#endif
