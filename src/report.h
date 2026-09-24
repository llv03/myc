#ifndef MYC_REPORT_H
#define MYC_REPORT_H

#include "common.h"
#include <stdarg.h>

typedef enum {
    REPORT_ERROR = 0,   /* parse / compile */
    REPORT_RUNTIME
} ReportPhase;

/* Print to stderr:
 *   myc: [file:][line:][col:] error: msg
 *   myc: [file:][line:][col:] runtime error: msg
 * file may be NULL; line/col <= 0 are omitted. */
void report_error(const char *file, int line, int col,
                  ReportPhase phase, const char *fmt, ...);
void report_errorv(const char *file, int line, int col,
                   ReportPhase phase, const char *fmt, va_list ap);

#endif
