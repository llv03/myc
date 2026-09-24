#include "vm.h"
#include "native_net.h"
#include "native_fs.h"
#include "native_os.h"
#include "native_graphics.h"
#include "report.h"
#include <math.h>

void vm_init(VM *vm) {
    vm->program = NULL;
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
}

void vm_free(VM *vm) {
    vm_init(vm);
}

static void reset_stack(VM *vm) {
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
}

static void runtime_error(VM *vm, const char *fmt, ...) {
    const char *file = NULL;
    int line = 0;
    if (vm->frame_count > 0) {
        CallFrame *frame = &vm->frames[vm->frame_count - 1];
        size_t offset = (size_t)(frame->ip - frame->func->chunk.code);
        if (offset > 0) offset--;
        line = frame->func->chunk.lines[offset];
        if (frame->func->source_path[0])
            file = frame->func->source_path;
    }
    va_list ap;
    va_start(ap, fmt);
    report_errorv(file, line, 0, REPORT_RUNTIME, fmt, ap);
    va_end(ap);
    for (int i = vm->frame_count - 1; i >= 0; i--)
        fprintf(stderr, "  in %s()\n", vm->frames[i].func->name);
    reset_stack(vm);
}

static void push(VM *vm, Value v) {
    if (vm->stack_top - vm->stack >= MAX_STACK) {
        runtime_error(vm, "stack overflow");
        return;
    }
    *vm->stack_top++ = v;
}

static Value pop(VM *vm) {
    return *--vm->stack_top;
}

static Value peek(VM *vm, int distance) {
    return vm->stack_top[-1 - distance];
}

static bool call_func(VM *vm, int func_id, int arg_count) {
    if (func_id < 0 || func_id >= vm->program->func_count) {
        runtime_error(vm, "invalid function");
        return false;
    }
    FuncObj *func = &vm->program->functions[func_id];
    if (func->kind != FUNC_USER) {
        runtime_error(vm, "cannot call native function object");
        return false;
    }
    if (arg_count != func->arity) {
        runtime_error(vm, "%s() expected %d argument%s but got %d",
                      func->name, func->arity,
                      func->arity == 1 ? "" : "s", arg_count);
        return false;
    }
    if (vm->frame_count == MAX_FRAMES) {
        runtime_error(vm, "stack overflow (too many calls)");
        return false;
    }
    CallFrame *frame = &vm->frames[vm->frame_count++];
    frame->func = func;
    frame->ip = func->chunk.code;
    frame->slots = vm->stack_top - arg_count - 1;
    return true;
}

static bool binary_arith(VM *vm, uint8_t op) {
    Value b = pop(vm);
    Value a = pop(vm);

    if (op == OP_ADD && (is_string(a) || is_string(b))) {
        ObjString *sa = is_string(a) ? as_string(a) : value_to_string(a);
        ObjString *sb = is_string(b) ? as_string(b) : value_to_string(b);
        int len = sa->length + sb->length;
        char *chars = malloc((size_t)len + 1);
        if (!chars) { runtime_error(vm, "out of memory"); return false; }
        memcpy(chars, sa->chars, (size_t)sa->length);
        memcpy(chars + sa->length, sb->chars, (size_t)sb->length);
        chars[len] = '\0';
        push(vm, obj_val((Obj *)obj_string_take(chars, len)));
        return true;
    }

    if (!is_number(a) || !is_number(b)) {
        runtime_error(vm, "operands must be numbers (got %s and %s)",
                      value_type_name(a), value_type_name(b));
        return false;
    }

    if (is_int(a) && is_int(b)) {
        int64_t ia = a.as.integer, ib = b.as.integer;
        int64_t r = 0;
        switch (op) {
            case OP_ADD: r = ia + ib; break;
            case OP_SUB: r = ia - ib; break;
            case OP_MUL: r = ia * ib; break;
            case OP_DIV:
                if (ib == 0) { runtime_error(vm, "division by zero"); return false; }
                r = ia / ib; break;
            case OP_MOD:
                if (ib == 0) { runtime_error(vm, "modulo by zero"); return false; }
                r = ia % ib; break;
            default: break;
        }
        push(vm, int_val(r));
        return true;
    }

    double da = as_double(a), db = as_double(b);
    double r = 0;
    switch (op) {
        case OP_ADD: r = da + db; break;
        case OP_SUB: r = da - db; break;
        case OP_MUL: r = da * db; break;
        case OP_DIV:
            if (db == 0.0) { runtime_error(vm, "division by zero"); return false; }
            r = da / db; break;
        case OP_MOD:
            if (db == 0.0) { runtime_error(vm, "modulo by zero"); return false; }
            r = fmod(da, db); break;
        default: break;
    }
    push(vm, float_val(r));
    return true;
}

static bool binary_cmp(VM *vm, uint8_t op) {
    Value b = pop(vm);
    Value a = pop(vm);
    if (op == OP_EQ) { push(vm, bool_val(values_equal(a, b))); return true; }
    if (op == OP_NE) { push(vm, bool_val(!values_equal(a, b))); return true; }
    if (!is_number(a) || !is_number(b)) {
        runtime_error(vm, "operands must be numbers (got %s and %s)",
                      value_type_name(a), value_type_name(b));
        return false;
    }
    double da = as_double(a), db = as_double(b);
    bool r = false;
    switch (op) {
        case OP_LT: r = da < db; break;
        case OP_LE: r = da <= db; break;
        case OP_GT: r = da > db; break;
        case OP_GE: r = da >= db; break;
        default: break;
    }
    push(vm, bool_val(r));
    return true;
}

InterpretResult vm_run(VM *vm, CompileResult *program) {
    vm->program = program;
    reset_stack(vm);
    gc_set_vm(vm);

    ObjFunction *main_fn = obj_function_new(program->main_index);
    push(vm, obj_val((Obj *)main_fn));
    if (!call_func(vm, program->main_index, 0))
        return INTERPRET_RUNTIME_ERROR;

    CallFrame *frame = &vm->frames[vm->frame_count - 1];

#define READ_BYTE()   (*frame->ip++)
#define READ_SHORT()  (frame->ip += 2, \
        (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->func->chunk.constants[READ_BYTE()])

    for (;;) {
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_CONSTANT: push(vm, READ_CONSTANT()); break;
            case OP_NIL:      push(vm, nil_val()); break;
            case OP_TRUE:     push(vm, bool_val(true)); break;
            case OP_FALSE:    push(vm, bool_val(false)); break;

            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
                if (!binary_arith(vm, instruction))
                    return INTERPRET_RUNTIME_ERROR;
                break;

            case OP_NEGATE: {
                Value v = pop(vm);
                if (is_int(v)) push(vm, int_val(-v.as.integer));
                else if (is_float(v)) push(vm, float_val(-v.as.floating));
                else {
                    runtime_error(vm, "operand must be a number (got %s)",
                                  value_type_name(v));
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_NOT:
                push(vm, bool_val(!is_truthy(pop(vm))));
                break;

            case OP_EQ: case OP_NE: case OP_LT: case OP_LE: case OP_GT: case OP_GE:
                if (!binary_cmp(vm, instruction))
                    return INTERPRET_RUNTIME_ERROR;
                break;

            case OP_PRINT:
                print_value(pop(vm));
                printf("\n");
                break;
            case OP_POP:
                pop(vm);
                break;

            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm, frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }

            case OP_JUMP:
                {
                    uint16_t off = READ_SHORT();
                    frame->ip += off;
                }
                break;
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (!is_truthy(peek(vm, 0))) frame->ip += offset;
                break;
            }
            case OP_LOOP:
                {
                    uint16_t off = READ_SHORT();
                    frame->ip -= off;
                }
                break;

            case OP_CLOSURE: {
                uint8_t fid = READ_BYTE();
                push(vm, obj_val((Obj *)obj_function_new((int)fid)));
                break;
            }

            case OP_CALL: {
                int arg_count = READ_BYTE();
                Value callee = peek(vm, arg_count);
                if (!is_function(callee)) {
                    runtime_error(vm, "can only call functions (got %s)",
                                  value_type_name(callee));
                    return INTERPRET_RUNTIME_ERROR;
                }
                {
                    int fid = as_function(callee)->func_id;
                    if (fid < 0 || fid >= vm->program->func_count) {
                        runtime_error(vm, "invalid function");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    FuncObj *fobj = &vm->program->functions[fid];
                    if (fobj->kind != FUNC_USER) {
                        if (arg_count != fobj->arity) {
                            runtime_error(vm,
                                "%s() expected %d argument%s but got %d",
                                fobj->name, fobj->arity,
                                fobj->arity == 1 ? "" : "s", arg_count);
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        Value args[8];
                        for (int i = arg_count - 1; i >= 0; i--)
                            args[i] = pop(vm);
                        pop(vm); /* callee */
                        Value result = nil_val();
                        switch (fobj->kind) {
                            case FUNC_NATIVE_NET_LISTEN:
                                result = native_net_listen(args[0]);
                                break;
                            case FUNC_NATIVE_NET_ACCEPT:
                                result = native_net_accept(args[0]);
                                break;
                            case FUNC_NATIVE_NET_RECV:
                                result = native_net_recv(args[0], args[1]);
                                break;
                            case FUNC_NATIVE_NET_SEND:
                                result = native_net_send(args[0], args[1]);
                                break;
                            case FUNC_NATIVE_NET_CLOSE:
                                result = native_net_close(args[0]);
                                break;
                            case FUNC_NATIVE_FS_READ:
                                result = native_fs_read(args[0]);
                                break;
                            case FUNC_NATIVE_FS_WRITE:
                                result = native_fs_write(args[0], args[1]);
                                break;
                            case FUNC_NATIVE_FS_APPEND:
                                result = native_fs_append(args[0], args[1]);
                                break;
                            case FUNC_NATIVE_FS_EXISTS:
                                result = native_fs_exists(args[0]);
                                break;
                            case FUNC_NATIVE_FS_REMOVE:
                                result = native_fs_remove(args[0]);
                                break;
                            case FUNC_NATIVE_MAP_KEYS:
                                result = native_map_keys(args[0]);
                                break;
                            case FUNC_NATIVE_OS_ARGS:
                                result = native_os_args();
                                break;
                            case FUNC_NATIVE_OS_SYSTEM:
                                result = native_os_system(args[0]);
                                break;
                            case FUNC_NATIVE_OS_CAPTURE:
                                result = native_os_capture(args[0]);
                                break;
                            case FUNC_NATIVE_OS_GETENV:
                                result = native_os_getenv(args[0]);
                                break;
                            case FUNC_NATIVE_OS_CWD:
                                result = native_os_cwd();
                                break;
                            case FUNC_NATIVE_GFX_AVAILABLE:
                                result = native_gfx_available();
                                break;
                            case FUNC_NATIVE_GFX_OPEN:
                                result = native_gfx_open(args[0], args[1], args[2]);
                                break;
                            case FUNC_NATIVE_GFX_TITLE:
                                result = native_gfx_title(args[0]);
                                break;
                            case FUNC_NATIVE_GFX_BG:
                                result = native_gfx_bg(args[0], args[1], args[2]);
                                break;
                            case FUNC_NATIVE_GFX_COLOR:
                                result = native_gfx_color(args[0], args[1], args[2]);
                                break;
                            case FUNC_NATIVE_GFX_TEXT:
                                result = native_gfx_text(args[0], args[1], args[2]);
                                break;
                            case FUNC_NATIVE_GFX_RUN:
                                result = native_gfx_run();
                                break;
                            default:
                                runtime_error(vm, "unknown native function");
                                return INTERPRET_RUNTIME_ERROR;
                        }
                        push(vm, result);
                        break;
                    }
                }
                if (!call_func(vm, as_function(callee)->func_id, arg_count))
                    return INTERPRET_RUNTIME_ERROR;
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_RETURN: {
                Value result = pop(vm);
                vm->frame_count--;
                if (vm->frame_count == 0) {
                    pop(vm);
                    return INTERPRET_OK;
                }
                vm->stack_top = frame->slots;
                push(vm, result);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_BUILD_LIST: {
                uint8_t count = READ_BYTE();
                ObjList *list = obj_list_new();
                Value *start = vm->stack_top - count;
                for (int i = 0; i < count; i++)
                    list_push(list, start[i]);
                vm->stack_top = start;
                push(vm, obj_val((Obj *)list));
                break;
            }

            case OP_BUILD_MAP: {
                uint8_t count = READ_BYTE();
                ObjMap *map = obj_map_new();
                Value *start = vm->stack_top - (count * 2);
                for (int i = 0; i < count; i++)
                    map_set(map, start[i * 2], start[i * 2 + 1]);
                vm->stack_top = start;
                push(vm, obj_val((Obj *)map));
                break;
            }

            case OP_BUILD_STRUCT: {
                uint8_t type_id = READ_BYTE();
                if (type_id >= (uint8_t)vm->program->struct_count) {
                    runtime_error(vm, "invalid struct type");
                    return INTERPRET_RUNTIME_ERROR;
                }
                StructDef *def = &vm->program->structs[type_id];
                ObjStruct *st = obj_struct_new((int)type_id, def->field_count);
                Value *start = vm->stack_top - def->field_count;
                for (int i = 0; i < def->field_count; i++)
                    st->fields[i] = start[i];
                vm->stack_top = start;
                push(vm, obj_val((Obj *)st));
                break;
            }

            case OP_INDEX_GET: {
                Value idx = pop(vm);
                Value coll = pop(vm);
                if (is_list(coll)) {
                    int64_t i;
                    if (!value_to_int(idx, &i)) {
                        runtime_error(vm, "list index must be int");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    Value out;
                    if (!list_get(as_list(coll), i, &out)) {
                        runtime_error(vm,
                            "list index %lld out of bounds (length %d)",
                            (long long)i, as_list(coll)->count);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(vm, out);
                } else if (is_map(coll)) {
                    Value out;
                    if (!map_get(as_map(coll), idx, &out))
                        push(vm, nil_val());
                    else
                        push(vm, out);
                } else if (is_string(coll)) {
                    int64_t i;
                    if (!value_to_int(idx, &i)) {
                        runtime_error(vm, "string index must be int");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    ObjString *s = as_string(coll);
                    if (i < 0 || i >= s->length) {
                        runtime_error(vm,
                            "string index %lld out of bounds (length %d)",
                            (long long)i, s->length);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(vm, obj_val((Obj *)obj_string_copy(s->chars + (int)i, 1)));
                } else {
                    runtime_error(vm,
                        "can only index list, map, or string (got %s)",
                        value_type_name(coll));
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_INDEX_SET: {
                Value val = pop(vm);
                Value idx = pop(vm);
                Value coll = pop(vm);
                if (is_list(coll)) {
                    int64_t i;
                    if (!value_to_int(idx, &i)) {
                        runtime_error(vm, "list index must be int");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    if (!list_set(as_list(coll), i, val)) {
                        runtime_error(vm,
                            "list index %lld out of bounds (length %d)",
                            (long long)i, as_list(coll)->count);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(vm, val);
                } else if (is_map(coll)) {
                    map_set(as_map(coll), idx, val);
                    push(vm, val);
                } else {
                    runtime_error(vm,
                        "can only index-assign list or map (got %s)",
                        value_type_name(coll));
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_GET_PROP: {
                Value namev = READ_CONSTANT();
                Value obj = pop(vm);
                if (!is_string(namev) || !is_struct(obj)) {
                    runtime_error(vm,
                        "property access requires a struct (got %s)",
                        value_type_name(obj));
                    return INTERPRET_RUNTIME_ERROR;
                }
                const char *name = as_string(namev)->chars;
                ObjStruct *st = as_struct(obj);
                if (st->type_id < 0 || st->type_id >= vm->program->struct_count) {
                    runtime_error(vm, "invalid struct");
                    return INTERPRET_RUNTIME_ERROR;
                }
                StructDef *def = &vm->program->structs[st->type_id];
                int fi = -1;
                for (int i = 0; i < def->field_count; i++) {
                    if (strcmp(def->fields[i], name) == 0) { fi = i; break; }
                }
                if (fi < 0) {
                    runtime_error(vm, "undefined property '%s' on %s",
                                  name, def->name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(vm, st->fields[fi]);
                break;
            }

            case OP_SET_PROP: {
                Value namev = READ_CONSTANT();
                Value val = pop(vm);
                Value obj = pop(vm);
                if (!is_string(namev) || !is_struct(obj)) {
                    runtime_error(vm,
                        "property assignment requires a struct (got %s)",
                        value_type_name(obj));
                    return INTERPRET_RUNTIME_ERROR;
                }
                const char *name = as_string(namev)->chars;
                ObjStruct *st = as_struct(obj);
                StructDef *def = &vm->program->structs[st->type_id];
                int fi = -1;
                for (int i = 0; i < def->field_count; i++) {
                    if (strcmp(def->fields[i], name) == 0) { fi = i; break; }
                }
                if (fi < 0) {
                    runtime_error(vm, "undefined property '%s' on %s",
                                  name, def->name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                st->fields[fi] = val;
                push(vm, val);
                break;
            }

            case OP_LEN: {
                Value v = pop(vm);
                if (is_string(v))
                    push(vm, int_val(as_string(v)->length));
                else if (is_list(v))
                    push(vm, int_val(as_list(v)->count));
                else if (is_map(v))
                    push(vm, int_val(as_map(v)->count));
                else {
                    runtime_error(vm,
                        "len() expects string, list, or map (got %s)",
                        value_type_name(v));
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_STR:
                push(vm, obj_val((Obj *)value_to_string(pop(vm))));
                break;

            case OP_TYPE: {
                const char *name = value_type_name(pop(vm));
                push(vm, obj_val((Obj *)obj_string_copy(name, (int)strlen(name))));
                break;
            }

            default:
                runtime_error(vm, "unknown opcode");
                return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
}
