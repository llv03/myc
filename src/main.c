#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "native_os.h"
#include "vm.h"

static void usage(const char *argv0) {
    fprintf(stderr, "Usage: %s [-d] <file.my> [args...]\n", argv0);
    fprintf(stderr, "  -d   disassemble bytecode, then run\n");
    fprintf(stderr, "  args are available to the script via os.args() / __args()\n");
}

int main(int argc, char **argv) {
    bool dump = false;
    const char *path = NULL;
    int script_i = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            dump = true;
        } else if (argv[i][0] == '-') {
            usage(argv[0]);
            return 1;
        } else {
            path = argv[i];
            script_i = i;
            break; /* remaining argv → script args (including path as [0]) */
        }
    }

    if (!path) {
        usage(argv[0]);
        return 1;
    }

    /* Script argv: path + user args. Myc flags like -d are stripped. */
    native_os_set_args(argc - script_i, &argv[script_i]);

    gc_init();

    CompileResult code;
    if (!compile_file(path, &code)) {
        compile_result_free(&code);
        gc_free_all();
        return 65;
    }

    if (dump) {
        disassemble_program(&code);
    }

    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_run(&vm, &code);
    vm_free(&vm);
    compile_result_free(&code);
    gc_free_all();

    if (result == INTERPRET_RUNTIME_ERROR) return 70;
    return 0;
}
