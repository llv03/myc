#ifndef MYC_VM_H
#define MYC_VM_H

#include "compiler.h"
#include "value.h"

typedef struct {
    FuncObj *func;
    uint8_t *ip;
    Value *slots; /* pointer into VM stack */
} CallFrame;

/* Incomplete type already typedef'd in value.h as VM: define the struct. */
struct VM {
    CompileResult *program;
    Value stack[MAX_STACK];
    Value *stack_top;
    CallFrame frames[MAX_FRAMES];
    int frame_count;
};

void vm_init(VM *vm);
void vm_free(VM *vm);
InterpretResult vm_run(VM *vm, CompileResult *program);

#endif
