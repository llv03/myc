#include "debug.h"

static int simple(const char *name, int offset) {
    printf("%-16s\n", name);
    return offset + 1;
}

static int constant_instr(const char *name, Chunk *chunk, int offset) {
    uint8_t idx = chunk->code[offset + 1];
    printf("%-16s %4d '", name, idx);
    if (idx < chunk->const_count)
        print_value(chunk->constants[idx]);
    printf("'\n");
    return offset + 2;
}

static int byte_instr(const char *name, Chunk *chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jump_instr(const char *name, int sign, Chunk *chunk, int offset) {
    uint16_t jump = (uint16_t)((chunk->code[offset + 1] << 8) |
                               chunk->code[offset + 2]);
    printf("%-16s %4d -> %d\n", name, offset,
           offset + 3 + sign * (int)jump);
    return offset + 3;
}

int disassemble_instruction(Chunk *chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
        printf("   | ");
    else
        printf("%4d ", chunk->lines[offset]);

    uint8_t op = chunk->code[offset];
    switch (op) {
        case OP_CONSTANT:      return constant_instr("OP_CONSTANT", chunk, offset);
        case OP_NIL:           return simple("OP_NIL", offset);
        case OP_TRUE:          return simple("OP_TRUE", offset);
        case OP_FALSE:         return simple("OP_FALSE", offset);
        case OP_ADD:           return simple("OP_ADD", offset);
        case OP_SUB:           return simple("OP_SUB", offset);
        case OP_MUL:           return simple("OP_MUL", offset);
        case OP_DIV:           return simple("OP_DIV", offset);
        case OP_MOD:           return simple("OP_MOD", offset);
        case OP_NEGATE:        return simple("OP_NEGATE", offset);
        case OP_NOT:           return simple("OP_NOT", offset);
        case OP_EQ:            return simple("OP_EQ", offset);
        case OP_NE:            return simple("OP_NE", offset);
        case OP_LT:            return simple("OP_LT", offset);
        case OP_LE:            return simple("OP_LE", offset);
        case OP_GT:            return simple("OP_GT", offset);
        case OP_GE:            return simple("OP_GE", offset);
        case OP_PRINT:         return simple("OP_PRINT", offset);
        case OP_POP:           return simple("OP_POP", offset);
        case OP_GET_LOCAL:     return byte_instr("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:     return byte_instr("OP_SET_LOCAL", chunk, offset);
        case OP_JUMP:          return jump_instr("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE: return jump_instr("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:          return jump_instr("OP_LOOP", -1, chunk, offset);
        case OP_CALL:          return byte_instr("OP_CALL", chunk, offset);
        case OP_RETURN:        return simple("OP_RETURN", offset);
        case OP_CLOSURE:       return byte_instr("OP_CLOSURE", chunk, offset);
        case OP_BUILD_LIST:    return byte_instr("OP_BUILD_LIST", chunk, offset);
        case OP_BUILD_MAP:     return byte_instr("OP_BUILD_MAP", chunk, offset);
        case OP_BUILD_STRUCT:  return byte_instr("OP_BUILD_STRUCT", chunk, offset);
        case OP_INDEX_GET:     return simple("OP_INDEX_GET", offset);
        case OP_INDEX_SET:     return simple("OP_INDEX_SET", offset);
        case OP_GET_PROP:      return constant_instr("OP_GET_PROP", chunk, offset);
        case OP_SET_PROP:      return constant_instr("OP_SET_PROP", chunk, offset);
        case OP_LEN:           return simple("OP_LEN", offset);
        case OP_STR:           return simple("OP_STR", offset);
        case OP_TYPE:          return simple("OP_TYPE", offset);
        default:
            printf("Unknown opcode %d\n", op);
            return offset + 1;
    }
}

void disassemble_chunk(Chunk *chunk, const char *name) {
    printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count; )
        offset = disassemble_instruction(chunk, offset);
}

void disassemble_program(CompileResult *prog) {
    for (int i = 0; i < prog->func_count; i++) {
        disassemble_chunk(&prog->functions[i].chunk, prog->functions[i].name);
        printf("\n");
    }
}
