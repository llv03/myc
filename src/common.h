#ifndef MYC_COMMON_H
#define MYC_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define UINT8_COUNT (UINT8_MAX + 1)

/* Soft caps for a student VM. */
#define MAX_LOCALS       256
#define MAX_CONSTANTS    256
#define MAX_FRAMES        64
#define MAX_STACK        512
#define MAX_FUNCS        128
#define MAX_PARAMS         8
#define MAX_NAME          64
#define MAX_FIELDS        16
#define MAX_STRUCTS       32
#define MAX_PATH         512
#define MAX_IMPORT_DEPTH  32

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

#endif
