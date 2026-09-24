#ifndef MYC_NATIVE_OS_H
#define MYC_NATIVE_OS_H

#include "value.h"

/* Process / CLI helpers for myc __args / __system / __capture / …
 * Commands run through /bin/sh -c (intentional shell). */

/* Store script argv after myc flag parsing. argv[0] = script path. */
void native_os_set_args(int argc, char **argv);

Value native_os_args(void);                 /* → list of strings */
Value native_os_system(Value cmd);          /* → int exit status; -1 if launch fails */
Value native_os_capture(Value cmd);         /* → map {status, out, err} | nil */
Value native_os_getenv(Value name);         /* → string | nil */
Value native_os_cwd(void);                  /* → string | nil */

#endif
