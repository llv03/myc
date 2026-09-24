#ifndef MYC_VALUE_H
#define MYC_VALUE_H

#include "common.h"

/* ---- tagged values -----------------------------------------------------
 * VAL_NIL | VAL_BOOL | VAL_INT | VAL_FLOAT | VAL_OBJ
 * Heap objects (OBJ_*): STRING, LIST, MAP, STRUCT, FUNCTION
 * GC: mark-sweep (see value.c). Chosen over refcount for cycle safety
 * with lists/maps that can form graphs.
 * ---------------------------------------------------------------------- */

typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_OBJ
} ValueType;

typedef enum {
    OBJ_STRING,
    OBJ_LIST,
    OBJ_MAP,
    OBJ_STRUCT,
    OBJ_FUNCTION
} ObjType;

typedef struct Obj {
    ObjType type;
    bool marked;
    struct Obj *next;
} Obj;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        int64_t integer;
        double floating;
        Obj *obj;
    } as;
} Value;

typedef struct {
    Obj obj;
    int length;
    char *chars;       /* null-terminated, owned */
    uint32_t hash;
} ObjString;

typedef struct {
    Obj obj;
    Value *items;
    int count;
    int capacity;
} ObjList;

typedef struct {
    Obj obj;
    Value *keys;
    Value *vals;
    int count;
    int capacity;
} ObjMap;

typedef struct {
    Obj obj;
    int type_id;       /* index into CompileResult.structs */
    Value *fields;
    int field_count;
} ObjStruct;

typedef struct {
    Obj obj;
    int func_id;       /* index into CompileResult.functions */
} ObjFunction;

/* value helpers */
static inline Value nil_val(void) {
    Value v; v.type = VAL_NIL; v.as.integer = 0; return v;
}
static inline Value bool_val(bool b) {
    Value v; v.type = VAL_BOOL; v.as.boolean = b; return v;
}
static inline Value int_val(int64_t i) {
    Value v; v.type = VAL_INT; v.as.integer = i; return v;
}
static inline Value float_val(double d) {
    Value v; v.type = VAL_FLOAT; v.as.floating = d; return v;
}
static inline Value obj_val(Obj *o) {
    Value v; v.type = VAL_OBJ; v.as.obj = o; return v;
}

static inline bool is_nil(Value v)    { return v.type == VAL_NIL; }
static inline bool is_bool(Value v)   { return v.type == VAL_BOOL; }
static inline bool is_int(Value v)    { return v.type == VAL_INT; }
static inline bool is_float(Value v)  { return v.type == VAL_FLOAT; }
static inline bool is_obj(Value v)    { return v.type == VAL_OBJ; }
static inline bool is_number(Value v) { return is_int(v) || is_float(v); }

static inline bool is_obj_type(Value v, ObjType t) {
    return is_obj(v) && v.as.obj->type == t;
}
static inline bool is_string(Value v) { return is_obj_type(v, OBJ_STRING); }
static inline bool is_list(Value v)   { return is_obj_type(v, OBJ_LIST); }
static inline bool is_map(Value v)    { return is_obj_type(v, OBJ_MAP); }
static inline bool is_struct(Value v) { return is_obj_type(v, OBJ_STRUCT); }
static inline bool is_function(Value v){ return is_obj_type(v, OBJ_FUNCTION); }

static inline ObjString   *as_string(Value v)   { return (ObjString *)v.as.obj; }
static inline ObjList     *as_list(Value v)     { return (ObjList *)v.as.obj; }
static inline ObjMap      *as_map(Value v)      { return (ObjMap *)v.as.obj; }
static inline ObjStruct   *as_struct(Value v)   { return (ObjStruct *)v.as.obj; }
static inline ObjFunction *as_function(Value v) { return (ObjFunction *)v.as.obj; }

/* GC / object allocation: VM registers itself as the GC owner. */
typedef struct VM VM;

void  gc_init(void);
void  gc_free_all(void);
void  gc_set_vm(VM *vm);
void  gc_collect(void);

ObjString   *obj_string_copy(const char *chars, int length);
ObjString   *obj_string_take(char *chars, int length);
ObjList     *obj_list_new(void);
ObjMap      *obj_map_new(void);
ObjStruct   *obj_struct_new(int type_id, int field_count);
ObjFunction *obj_function_new(int func_id);

void list_push(ObjList *list, Value v);
bool list_get(ObjList *list, int64_t idx, Value *out);
bool list_set(ObjList *list, int64_t idx, Value v);

bool map_get(ObjMap *map, Value key, Value *out);
void map_set(ObjMap *map, Value key, Value v);
bool values_equal(Value a, Value b);
bool is_truthy(Value v);
double as_double(Value v);          /* int or float → double; else 0 */
bool value_to_int(Value v, int64_t *out);

void print_value(Value v);
void print_value_repr(Value v);     /* for nested printing */
const char *value_type_name(Value v);

/* Build a heap string from a value (for str() builtin). Caller owns via GC. */
ObjString *value_to_string(Value v);

uint32_t hash_string(const char *chars, int length);
uint32_t hash_value(Value v);

#endif
