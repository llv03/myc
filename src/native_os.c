#include "native_os.h"

#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define OS_MAX_CAPTURE (8 * 1024 * 1024)
#define OS_CMD_SCRATCH 4096

static int g_argc = 0;
static char **g_argv = NULL; /* not owned; points into main's argv */

void native_os_set_args(int argc, char **argv) {
    g_argc = argc;
    g_argv = argv;
}

static const char *cmd_cstr(Value cmd_v, char *scratch, size_t scratch_n) {
    if (!is_string(cmd_v)) return NULL;
    ObjString *s = as_string(cmd_v);
    if ((size_t)s->length + 1 > scratch_n) return NULL;
    memcpy(scratch, s->chars, (size_t)s->length);
    scratch[s->length] = '\0';
    return scratch;
}

static int wait_status_to_exit(int st) {
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    if (WIFSIGNALED(st)) return 128 + WTERMSIG(st);
    return -1;
}

Value native_os_args(void) {
    ObjList *list = obj_list_new();
    for (int i = 0; i < g_argc; i++) {
        const char *a = g_argv[i] ? g_argv[i] : "";
        list_push(list, obj_val((Obj *)obj_string_copy(a, (int)strlen(a))));
    }
    return obj_val((Obj *)list);
}

Value native_os_system(Value cmd_v) {
    char cmd[OS_CMD_SCRATCH];
    if (!cmd_cstr(cmd_v, cmd, sizeof(cmd))) return int_val(-1);

    int st = system(cmd);
    if (st == -1) return int_val(-1);
    return int_val(wait_status_to_exit(st));
}

/* Growable byte buffer for capture. */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

static bool buf_init(Buf *b) {
    b->data = malloc(256);
    if (!b->data) return false;
    b->len = 0;
    b->cap = 256;
    b->data[0] = '\0';
    return true;
}

static bool buf_append(Buf *b, const char *p, size_t n) {
    if (b->len + n > OS_MAX_CAPTURE) return false;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap;
        while (nc < b->len + n + 1) nc *= 2;
        if (nc > OS_MAX_CAPTURE + 1) nc = OS_MAX_CAPTURE + 1;
        char *nd = realloc(b->data, nc);
        if (!nd) return false;
        b->data = nd;
        b->cap = nc;
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

static void buf_free(Buf *b) {
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

static Value make_capture_map(int status, Buf *out, Buf *err) {
    ObjString *out_s = obj_string_take(out->data, (int)out->len);
    out->data = NULL; /* ownership transferred */
    ObjString *err_s = obj_string_take(err->data, (int)err->len);
    err->data = NULL;

    ObjMap *m = obj_map_new();
    map_set(m, obj_val((Obj *)obj_string_copy("status", 6)), int_val(status));
    map_set(m, obj_val((Obj *)obj_string_copy("out", 3)), obj_val((Obj *)out_s));
    map_set(m, obj_val((Obj *)obj_string_copy("err", 3)), obj_val((Obj *)err_s));
    return obj_val((Obj *)m);
}

Value native_os_capture(Value cmd_v) {
    char cmd[OS_CMD_SCRATCH];
    if (!cmd_cstr(cmd_v, cmd, sizeof(cmd))) return nil_val();

    int out_pipe[2];
    int err_pipe[2];
    if (pipe(out_pipe) != 0) return nil_val();
    if (pipe(err_pipe) != 0) {
        close(out_pipe[0]);
        close(out_pipe[1]);
        return nil_val();
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return nil_val();
    }

    if (pid == 0) {
        /* Child: sh -c cmd, stdout/stderr → pipes */
        close(out_pipe[0]);
        close(err_pipe[0]);
        if (dup2(out_pipe[1], STDOUT_FILENO) < 0) _exit(127);
        if (dup2(err_pipe[1], STDERR_FILENO) < 0) _exit(127);
        close(out_pipe[1]);
        close(err_pipe[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    /* Parent */
    close(out_pipe[1]);
    close(err_pipe[1]);

    Buf outb = {0}, errb = {0};
    if (!buf_init(&outb) || !buf_init(&errb)) {
        buf_free(&outb);
        buf_free(&errb);
        close(out_pipe[0]);
        close(err_pipe[0]);
        waitpid(pid, NULL, 0);
        return nil_val();
    }

    int out_open = 1, err_open = 1;
    while (out_open || err_open) {
        struct pollfd pfds[2];
        int nf = 0;
        int out_ix = -1, err_ix = -1;
        if (out_open) {
            out_ix = nf;
            pfds[nf].fd = out_pipe[0];
            pfds[nf].events = POLLIN;
            nf++;
        }
        if (err_open) {
            err_ix = nf;
            pfds[nf].fd = err_pipe[0];
            pfds[nf].events = POLLIN;
            nf++;
        }
        int pr = poll(pfds, (nfds_t)nf, -1);
        if (pr < 0) {
            if (errno == EINTR) continue;
            break;
        }
        char tmp[4096];
        if (out_ix >= 0 && (pfds[out_ix].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t n = read(out_pipe[0], tmp, sizeof(tmp));
            if (n > 0) {
                if (!buf_append(&outb, tmp, (size_t)n)) {
                    /* Cap exceeded: keep reading to drain but stop storing */
                }
            } else {
                out_open = 0;
                close(out_pipe[0]);
            }
        }
        if (err_ix >= 0 && (pfds[err_ix].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t n = read(err_pipe[0], tmp, sizeof(tmp));
            if (n > 0) {
                if (!buf_append(&errb, tmp, (size_t)n)) {
                    /* Cap exceeded */
                }
            } else {
                err_open = 0;
                close(err_pipe[0]);
            }
        }
    }

    int st = 0;
    if (waitpid(pid, &st, 0) < 0) {
        buf_free(&outb);
        buf_free(&errb);
        return nil_val();
    }

    return make_capture_map(wait_status_to_exit(st), &outb, &errb);
}

Value native_os_getenv(Value name_v) {
    char name[256];
    if (!cmd_cstr(name_v, name, sizeof(name))) return nil_val();
    const char *v = getenv(name);
    if (!v) return nil_val();
    return obj_val((Obj *)obj_string_copy(v, (int)strlen(v)));
}

Value native_os_cwd(void) {
    char buf[MAX_PATH];
    if (!getcwd(buf, sizeof(buf))) return nil_val();
    return obj_val((Obj *)obj_string_copy(buf, (int)strlen(buf)));
}
