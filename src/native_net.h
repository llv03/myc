#ifndef MYC_NATIVE_NET_H
#define MYC_NATIVE_NET_H

#include "value.h"

/* POSIX TCP helpers for myc __net_* natives.
 * IDs are raw socket file descriptors (int values).
 * Returns nil on failure where noted. */

Value native_net_listen(Value port);           /* → int fd | nil */
Value native_net_accept(Value server);         /* → int fd | nil (blocking) */
Value native_net_recv(Value conn, Value maxn); /* → string (maybe "") */
Value native_net_send(Value conn, Value s);    /* → int bytes | nil */
Value native_net_close(Value id);              /* → true | nil */

#endif
