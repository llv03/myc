#ifndef MYC_CHUNK_H
#define MYC_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NEGATE,
    OP_NOT,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_PRINT,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_RETURN,
    OP_CLOSURE,
    OP_BUILD_LIST,   /* operand: count; pop N values, push list */
    OP_BUILD_MAP,    /* operand: count; pop 2N (k,v)*count, push map */
    OP_BUILD_STRUCT, /* operand: type_id; pop field_count values */
    OP_INDEX_GET,    /* pop idx, pop coll -> push coll[idx] */
    OP_INDEX_SET,    /* pop val, pop idx, pop coll -> push val; coll[idx]=val */
    OP_GET_PROP,     /* operand: name const idx; pop obj, push obj.name */
    OP_SET_PROP,     /* operand: name const idx; pop val, pop obj, push val */
    OP_LEN,          /* pop x -> push len(x) */
    OP_STR,          /* pop x -> push str(x) */
    OP_TYPE          /* pop x -> push type name string */
} OpCode;

typedef struct {
    uint8_t *code;
    int *lines;
    int count;
    int capacity;
    Value *constants;
    int const_count;
    int const_capacity;
} Chunk;

void chunk_init(Chunk *c);
void chunk_free(Chunk *c);
void chunk_write(Chunk *c, uint8_t byte, int line);
int  chunk_add_constant(Chunk *c, Value value);

#endif
