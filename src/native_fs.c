#include "native_fs.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define FS_MAX_READ (8 * 1024 * 1024)

static const char *path_cstr(Value path_v, char *scratch, size_t scratch_n) {
    if (!is_string(path_v)) return NULL;
    ObjString *s = as_string(path_v);
    if ((size_t)s->length + 1 > scratch_n) return NULL;
    memcpy(scratch, s->chars, (size_t)s->length);
    scratch[s->length] = '\0';
    return scratch;
}

Value native_fs_read(Value path_v) {
    char path[MAX_PATH];
    if (!path_cstr(path_v, path, sizeof(path))) return nil_val();

    FILE *f = fopen(path, "rb");
    if (!f) return nil_val();

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return nil_val();
    }
    long sz = ftell(f);
    if (sz < 0 || sz > FS_MAX_READ) {
        fclose(f);
        return nil_val();
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return nil_val();
    }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return nil_val();
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return obj_val((Obj *)obj_string_take(buf, (int)n));
}

static Value write_mode(Value path_v, Value contents_v, const char *mode) {
    char path[MAX_PATH];
    if (!path_cstr(path_v, path, sizeof(path))) return bool_val(false);
    if (!is_string(contents_v)) return bool_val(false);

    ObjString *s = as_string(contents_v);
    FILE *f = fopen(path, mode);
    if (!f) return bool_val(false);

    size_t n = fwrite(s->chars, 1, (size_t)s->length, f);
    int err = ferror(f);
    if (fclose(f) != 0) return bool_val(false);
    if (err || n != (size_t)s->length) return bool_val(false);
    return bool_val(true);
}

Value native_fs_write(Value path_v, Value contents_v) {
    return write_mode(path_v, contents_v, "wb");
}

Value native_fs_append(Value path_v, Value contents_v) {
    return write_mode(path_v, contents_v, "ab");
}

Value native_fs_exists(Value path_v) {
    char path[MAX_PATH];
    if (!path_cstr(path_v, path, sizeof(path))) return bool_val(false);
    struct stat st;
    if (stat(path, &st) != 0) return bool_val(false);
    return bool_val(true);
}

Value native_fs_remove(Value path_v) {
    char path[MAX_PATH];
    if (!path_cstr(path_v, path, sizeof(path))) return bool_val(false);
    if (unlink(path) != 0) return bool_val(false);
    return bool_val(true);
}

Value native_map_keys(Value map_v) {
    if (!is_map(map_v)) return nil_val();
    ObjMap *map = as_map(map_v);
    ObjList *list = obj_list_new();
    for (int i = 0; i < map->count; i++)
        list_push(list, map->keys[i]);
    return obj_val((Obj *)list);
}
