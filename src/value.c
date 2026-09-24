#include "value.h"
#include "vm.h"
#include "compiler.h"

/* GC state */

static Obj *gc_objects = NULL;
static size_t gc_bytes_allocated = 0;
static size_t gc_next_collect = 1024 * 1024;
static VM *gc_vm = NULL;

void gc_init(void) {
    gc_objects = NULL;
    gc_bytes_allocated = 0;
    gc_next_collect = 1024 * 1024;
    gc_vm = NULL;
}

void gc_set_vm(VM *vm) {
    gc_vm = vm;
}

static void *gc_alloc(size_t size) {
    gc_bytes_allocated += size;
    if (gc_bytes_allocated > gc_next_collect) {
        gc_collect();
    }
    void *p = malloc(size);
    if (!p) {
        gc_collect();
        p = malloc(size);
        if (!p) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
    }
    return p;
}

static Obj *alloc_obj(size_t size, ObjType type) {
    Obj *obj = gc_alloc(size);
    obj->type = type;
    obj->marked = false;
    obj->next = gc_objects;
    gc_objects = obj;
    return obj;
}

/* mark */

static void mark_value(Value v);
static void mark_obj(Obj *obj);

static void mark_value(Value v) {
    if (is_obj(v)) mark_obj(v.as.obj);
}

static void mark_obj(Obj *obj) {
    if (!obj || obj->marked) return;
    obj->marked = true;
    switch (obj->type) {
        case OBJ_STRING:
        case OBJ_FUNCTION:
            break;
        case OBJ_LIST: {
            ObjList *list = (ObjList *)obj;
            for (int i = 0; i < list->count; i++)
                mark_value(list->items[i]);
            break;
        }
        case OBJ_MAP: {
            ObjMap *map = (ObjMap *)obj;
            for (int i = 0; i < map->count; i++) {
                mark_value(map->keys[i]);
                mark_value(map->vals[i]);
            }
            break;
        }
        case OBJ_STRUCT: {
            ObjStruct *s = (ObjStruct *)obj;
            for (int i = 0; i < s->field_count; i++)
                mark_value(s->fields[i]);
            break;
        }
    }
}

static void free_obj(Obj *obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString *s = (ObjString *)obj;
            free(s->chars);
            free(s);
            break;
        }
        case OBJ_LIST: {
            ObjList *list = (ObjList *)obj;
            free(list->items);
            free(list);
            break;
        }
        case OBJ_MAP: {
            ObjMap *map = (ObjMap *)obj;
            free(map->keys);
            free(map->vals);
            free(map);
            break;
        }
        case OBJ_STRUCT: {
            ObjStruct *s = (ObjStruct *)obj;
            free(s->fields);
            free(s);
            break;
        }
        case OBJ_FUNCTION:
            free(obj);
            break;
    }
}

void gc_collect(void) {
    /* Mark roots from VM stack + call frames' slots + chunk constants. */
    if (gc_vm) {
        for (Value *slot = gc_vm->stack; slot < gc_vm->stack_top; slot++)
            mark_value(*slot);
        if (gc_vm->program) {
            CompileResult *prog = gc_vm->program;
            for (int f = 0; f < prog->func_count; f++) {
                Chunk *chunk = &prog->functions[f].chunk;
                for (int i = 0; i < chunk->const_count; i++)
                    mark_value(chunk->constants[i]);
            }
            for (int i = 0; i < prog->native_fn_count; i++) {
                if (prog->native_fns[i])
                    mark_obj((Obj *)prog->native_fns[i]);
            }
        }
    }

    /* Sweep */
    Obj **ptr = &gc_objects;
    while (*ptr) {
        if (!(*ptr)->marked) {
            Obj *unreached = *ptr;
            *ptr = unreached->next;
            free_obj(unreached);
        } else {
            (*ptr)->marked = false;
            ptr = &(*ptr)->next;
        }
    }
    gc_next_collect = gc_bytes_allocated < 1024 ? 1024 * 1024
                                                : gc_bytes_allocated * 2;
}

void gc_free_all(void) {
    Obj *obj = gc_objects;
    while (obj) {
        Obj *next = obj->next;
        free_obj(obj);
        obj = next;
    }
    gc_objects = NULL;
    gc_bytes_allocated = 0;
    gc_vm = NULL;
}

/* hashing */

uint32_t hash_string(const char *chars, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)chars[i];
        hash *= 16777619u;
    }
    return hash;
}

uint32_t hash_value(Value v) {
    switch (v.type) {
        case VAL_NIL:   return 0;
        case VAL_BOOL:  return v.as.boolean ? 1 : 2;
        case VAL_INT: {
            uint64_t x = (uint64_t)v.as.integer;
            return (uint32_t)(x ^ (x >> 32));
        }
        case VAL_FLOAT: {
            uint64_t x;
            memcpy(&x, &v.as.floating, sizeof(x));
            return (uint32_t)(x ^ (x >> 32));
        }
        case VAL_OBJ:
            if (v.as.obj->type == OBJ_STRING)
                return ((ObjString *)v.as.obj)->hash;
            return (uint32_t)(uintptr_t)v.as.obj;
    }
    return 0;
}

/* constructors */

ObjString *obj_string_copy(const char *chars, int length) {
    char *heap = malloc((size_t)length + 1);
    if (!heap) { fprintf(stderr, "oom\n"); exit(1); }
    memcpy(heap, chars, (size_t)length);
    heap[length] = '\0';
    return obj_string_take(heap, length);
}

ObjString *obj_string_take(char *chars, int length) {
    ObjString *s = (ObjString *)alloc_obj(sizeof(ObjString), OBJ_STRING);
    s->length = length;
    s->chars = chars;
    s->hash = hash_string(chars, length);
    return s;
}

ObjList *obj_list_new(void) {
    ObjList *list = (ObjList *)alloc_obj(sizeof(ObjList), OBJ_LIST);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

ObjMap *obj_map_new(void) {
    ObjMap *map = (ObjMap *)alloc_obj(sizeof(ObjMap), OBJ_MAP);
    map->keys = NULL;
    map->vals = NULL;
    map->count = 0;
    map->capacity = 0;
    return map;
}

ObjStruct *obj_struct_new(int type_id, int field_count) {
    ObjStruct *s = (ObjStruct *)alloc_obj(sizeof(ObjStruct), OBJ_STRUCT);
    s->type_id = type_id;
    s->field_count = field_count;
    s->fields = calloc((size_t)field_count, sizeof(Value));
    if (!s->fields && field_count > 0) { fprintf(stderr, "oom\n"); exit(1); }
    for (int i = 0; i < field_count; i++)
        s->fields[i] = nil_val();
    return s;
}

ObjFunction *obj_function_new(int func_id) {
    ObjFunction *f = (ObjFunction *)alloc_obj(sizeof(ObjFunction), OBJ_FUNCTION);
    f->func_id = func_id;
    return f;
}

/* list / map ops */

void list_push(ObjList *list, Value v) {
    if (list->count + 1 > list->capacity) {
        int n = list->capacity < 8 ? 8 : list->capacity * 2;
        Value *items = realloc(list->items, (size_t)n * sizeof(Value));
        if (!items) { fprintf(stderr, "oom\n"); exit(1); }
        list->items = items;
        list->capacity = n;
    }
    list->items[list->count++] = v;
}

bool list_get(ObjList *list, int64_t idx, Value *out) {
    if (idx < 0 || idx >= list->count) return false;
    *out = list->items[idx];
    return true;
}

bool list_set(ObjList *list, int64_t idx, Value v) {
    if (idx < 0) return false;
    /* Allow a[len(a)] = x as append so myc can grow lists. */
    if (idx == list->count) {
        list_push(list, v);
        return true;
    }
    if (idx >= list->count) return false;
    list->items[idx] = v;
    return true;
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type) {
        /* int/float numeric equality */
        if (is_number(a) && is_number(b))
            return as_double(a) == as_double(b);
        return false;
    }
    switch (a.type) {
        case VAL_NIL:   return true;
        case VAL_BOOL:  return a.as.boolean == b.as.boolean;
        case VAL_INT:   return a.as.integer == b.as.integer;
        case VAL_FLOAT: return a.as.floating == b.as.floating;
        case VAL_OBJ:
            if (a.as.obj->type != b.as.obj->type) return false;
            if (a.as.obj->type == OBJ_STRING) {
                ObjString *sa = (ObjString *)a.as.obj;
                ObjString *sb = (ObjString *)b.as.obj;
                return sa->length == sb->length &&
                       memcmp(sa->chars, sb->chars, (size_t)sa->length) == 0;
            }
            if (a.as.obj->type == OBJ_FUNCTION)
                return ((ObjFunction *)a.as.obj)->func_id ==
                       ((ObjFunction *)b.as.obj)->func_id;
            return a.as.obj == b.as.obj;
    }
    return false;
}

bool map_get(ObjMap *map, Value key, Value *out) {
    for (int i = 0; i < map->count; i++) {
        if (values_equal(map->keys[i], key)) {
            *out = map->vals[i];
            return true;
        }
    }
    return false;
}

void map_set(ObjMap *map, Value key, Value v) {
    for (int i = 0; i < map->count; i++) {
        if (values_equal(map->keys[i], key)) {
            map->vals[i] = v;
            return;
        }
    }
    if (map->count + 1 > map->capacity) {
        int n = map->capacity < 8 ? 8 : map->capacity * 2;
        Value *keys = realloc(map->keys, (size_t)n * sizeof(Value));
        Value *vals = realloc(map->vals, (size_t)n * sizeof(Value));
        if (!keys || !vals) { fprintf(stderr, "oom\n"); exit(1); }
        map->keys = keys;
        map->vals = vals;
        map->capacity = n;
    }
    map->keys[map->count] = key;
    map->vals[map->count] = v;
    map->count++;
}

/* Truthiness: nil, false, 0, 0.0, "", [], {} are falsey; else truthy. */
bool is_truthy(Value v) {
    switch (v.type) {
        case VAL_NIL:   return false;
        case VAL_BOOL:  return v.as.boolean;
        case VAL_INT:   return v.as.integer != 0;
        case VAL_FLOAT: return v.as.floating != 0.0;
        case VAL_OBJ:
            switch (v.as.obj->type) {
                case OBJ_STRING: return ((ObjString *)v.as.obj)->length > 0;
                case OBJ_LIST:   return ((ObjList *)v.as.obj)->count > 0;
                case OBJ_MAP:    return ((ObjMap *)v.as.obj)->count > 0;
                default:         return true;
            }
    }
    return false;
}

double as_double(Value v) {
    if (is_int(v)) return (double)v.as.integer;
    if (is_float(v)) return v.as.floating;
    return 0.0;
}

bool value_to_int(Value v, int64_t *out) {
    if (is_int(v)) { *out = v.as.integer; return true; }
    if (is_float(v)) { *out = (int64_t)v.as.floating; return true; }
    return false;
}

const char *value_type_name(Value v) {
    switch (v.type) {
        case VAL_NIL:   return "nil";
        case VAL_BOOL:  return "bool";
        case VAL_INT:   return "int";
        case VAL_FLOAT: return "float";
        case VAL_OBJ:
            switch (v.as.obj->type) {
                case OBJ_STRING:   return "string";
                case OBJ_LIST:     return "list";
                case OBJ_MAP:      return "map";
                case OBJ_STRUCT:   return "struct";
                case OBJ_FUNCTION: return "function";
            }
    }
    return "unknown";
}

static void print_value_depth(Value v, int depth);

void print_value_repr(Value v) {
    print_value_depth(v, 0);
}

void print_value(Value v) {
    print_value_depth(v, 0);
}

static void print_value_depth(Value v, int depth) {
    if (depth > 8) { printf("..."); return; }
    switch (v.type) {
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_BOOL:
            printf(v.as.boolean ? "true" : "false");
            break;
        case VAL_INT:
            printf("%lld", (long long)v.as.integer);
            break;
        case VAL_FLOAT: {
            double d = v.as.floating;
            if (d == (double)(int64_t)d && d < 1e15 && d > -1e15)
                printf("%g.0", d);
            else
                printf("%g", d);
            break;
        }
        case VAL_OBJ:
            switch (v.as.obj->type) {
                case OBJ_STRING:
                    printf("%s", ((ObjString *)v.as.obj)->chars);
                    break;
                case OBJ_LIST: {
                    ObjList *list = (ObjList *)v.as.obj;
                    printf("[");
                    for (int i = 0; i < list->count; i++) {
                        if (i) printf(", ");
                        print_value_depth(list->items[i], depth + 1);
                    }
                    printf("]");
                    break;
                }
                case OBJ_MAP: {
                    ObjMap *map = (ObjMap *)v.as.obj;
                    printf("{");
                    for (int i = 0; i < map->count; i++) {
                        if (i) printf(", ");
                        if (is_string(map->keys[i])) {
                            printf("\"%s\"", as_string(map->keys[i])->chars);
                        } else {
                            print_value_depth(map->keys[i], depth + 1);
                        }
                        printf(": ");
                        print_value_depth(map->vals[i], depth + 1);
                    }
                    printf("}");
                    break;
                }
                case OBJ_STRUCT: {
                    ObjStruct *s = (ObjStruct *)v.as.obj;
                    const char *name = "struct";
                    if (gc_vm && gc_vm->program &&
                        s->type_id >= 0 &&
                        s->type_id < gc_vm->program->struct_count) {
                        name = gc_vm->program->structs[s->type_id].name;
                    }
                    printf("%s {", name);
                    if (gc_vm && gc_vm->program &&
                        s->type_id >= 0 &&
                        s->type_id < gc_vm->program->struct_count) {
                        StructDef *def = &gc_vm->program->structs[s->type_id];
                        for (int i = 0; i < s->field_count; i++) {
                            if (i) printf(", ");
                            printf("%s: ", def->fields[i]);
                            print_value_depth(s->fields[i], depth + 1);
                        }
                    } else {
                        for (int i = 0; i < s->field_count; i++) {
                            if (i) printf(", ");
                            print_value_depth(s->fields[i], depth + 1);
                        }
                    }
                    printf("}");
                    break;
                }
                case OBJ_FUNCTION:
                    printf("<fn:%d>", ((ObjFunction *)v.as.obj)->func_id);
                    break;
            }
            break;
    }
}

ObjString *value_to_string(Value v) {
    char buf[128];
    switch (v.type) {
        case VAL_NIL:
            return obj_string_copy("nil", 3);
        case VAL_BOOL:
            return v.as.boolean ? obj_string_copy("true", 4)
                                : obj_string_copy("false", 5);
        case VAL_INT:
            snprintf(buf, sizeof(buf), "%lld", (long long)v.as.integer);
            return obj_string_copy(buf, (int)strlen(buf));
        case VAL_FLOAT:
            snprintf(buf, sizeof(buf), "%g", v.as.floating);
            return obj_string_copy(buf, (int)strlen(buf));
        case VAL_OBJ:
            if (v.as.obj->type == OBJ_STRING)
                return (ObjString *)v.as.obj;
            {
                /* Use a temp buffer via open_memstream if available, else simple. */
                const char *name = value_type_name(v);
                return obj_string_copy(name, (int)strlen(name));
            }
    }
    return obj_string_copy("?", 1);
}
