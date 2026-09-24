#include "chunk.h"

void chunk_init(Chunk *c) {
    c->code = NULL;
    c->lines = NULL;
    c->count = 0;
    c->capacity = 0;
    c->constants = NULL;
    c->const_count = 0;
    c->const_capacity = 0;
}

void chunk_free(Chunk *c) {
    free(c->code);
    free(c->lines);
    free(c->constants);
    chunk_init(c);
}

void chunk_write(Chunk *c, uint8_t byte, int line) {
    if (c->count + 1 > c->capacity) {
        int n = c->capacity < 8 ? 8 : c->capacity * 2;
        uint8_t *code = realloc(c->code, (size_t)n * sizeof(uint8_t));
        int *lines = realloc(c->lines, (size_t)n * sizeof(int));
        if (!code || !lines) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        c->code = code;
        c->lines = lines;
        c->capacity = n;
    }
    c->code[c->count] = byte;
    c->lines[c->count] = line;
    c->count++;
}

int chunk_add_constant(Chunk *c, Value value) {
    if (c->const_count >= MAX_CONSTANTS) {
        fprintf(stderr, "too many constants in chunk\n");
        exit(1);
    }
    if (c->const_count + 1 > c->const_capacity) {
        int n = c->const_capacity < 8 ? 8 : c->const_capacity * 2;
        Value *cs = realloc(c->constants, (size_t)n * sizeof(Value));
        if (!cs) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        c->constants = cs;
        c->const_capacity = n;
    }
    c->constants[c->const_count] = value;
    return c->const_count++;
}
