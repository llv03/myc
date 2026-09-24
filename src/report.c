#include "report.h"

void report_errorv(const char *file, int line, int col,
                   ReportPhase phase, const char *fmt, va_list ap) {
    fputs("myc: ", stderr);
    if (file && file[0]) {
        fputs(file, stderr);
        fputc(':', stderr);
    }
    if (line > 0) {
        fprintf(stderr, "%d:", line);
        if (col > 0)
            fprintf(stderr, "%d:", col);
    }
    if (phase == REPORT_RUNTIME)
        fputs(" runtime error: ", stderr);
    else
        fputs(" error: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void report_error(const char *file, int line, int col,
                  ReportPhase phase, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    report_errorv(file, line, col, phase, fmt, ap);
    va_end(ap);
}
