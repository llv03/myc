#ifndef MYC_NATIVE_FS_H
#define MYC_NATIVE_FS_H

#include "value.h"

/* POSIX text-file helpers for myc __fs_* natives.
 * Paths are relative to the process cwd (or absolute).
 * Read loads the whole file into a myc string (cap 8 MiB). */

Value native_fs_read(Value path);                 /* → string | nil */
Value native_fs_write(Value path, Value contents); /* → true | false */
Value native_fs_append(Value path, Value contents);/* → true | false */
Value native_fs_exists(Value path);               /* → bool */
Value native_fs_remove(Value path);               /* → bool */

/* Map iteration helper for pure-myc json/csv. */
Value native_map_keys(Value map_v);               /* → list of keys | nil */

#endif
