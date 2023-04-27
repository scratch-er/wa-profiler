#include <signal.h>
#include <stdlib.h>
#include <wasm.h>
#include <wasi.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define own

void print_usage();

int main(int argc, char **argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    // initialize wasm runtime context
    wasm_engine_t *engine = wasm_engine_new();
    wasm_store_t *store = wasm_store_new(engine);

    // load module file
    FILE* module_file = fopen(argv[1], "rb");
    if (module_file == NULL) {
        printf("error: failed to load module %s\n", argv[1]);
        return 2;
    }
    fseek(module_file, 0L, SEEK_END);
    size_t file_size = ftell(module_file);
    fseek(module_file, 0L, SEEK_SET);
    own wasm_byte_vec_t module_binary;
    wasm_byte_vec_new_uninitialized(&module_binary, file_size);
    fread(module_binary.data, file_size, 1, module_file);
    fclose(module_file);

    // validate the module
    if (! wasm_module_validate(store, &module_binary)) {
        wasm_byte_vec_delete(&module_binary);
        printf("error: %s is not a valid wasm module\n", argv[1]);
        return 3;
    }

    // construct the module
    own wasm_module_t *module = wasm_module_new(store, &module_binary);
    wasm_byte_vec_delete(&module_binary);
    if (module == NULL) {
        printf("error: failed to construct module\n");
        return 1;
    }

    // find the _start function
    own wasm_exporttype_vec_t exports;
    int start_func_index = -1;
    wasm_module_exports(module, &exports);
    for (int i=0; i<exports.size; ++i) {
        const wasm_name_t *name = wasm_exporttype_name(exports.data[i]);
        if (strncmp("_start", name->data, 6) == 0) {
            // check whether _start is a function
            const wasm_externtype_t *type = wasm_exporttype_type(exports.data[i]);
            wasm_externkind_t kind = wasm_externtype_kind(type);
            if (kind != WASM_EXTERN_FUNC) {
                wasm_exporttype_vec_delete(&exports);
                printf("error: _start is not a function\n");
                return 4;
            }
            // check the signature of _start()
            const wasm_functype_t *functype =  wasm_externtype_as_functype_const(type);
            const wasm_valtype_vec_t *params_type = wasm_functype_params(functype);
            const wasm_valtype_vec_t *results_type = wasm_functype_results(functype);
            if (params_type->size!=0 || results_type->size!=0) {
                printf("wrong function signature of _start()\n");
                return 1;
            }
            start_func_index = i;
            break;
        }
    }
    if (start_func_index == -1) {
        printf("error: _start not found\n");
        return 5;
    }
    wasm_exporttype_vec_delete(&exports);

    // instantinate the module
    wasm_extern_vec_t imports = WASM_EMPTY_VEC;
    own wasm_trap_t* trap = NULL;
    own wasm_instance_t *inst = wasm_instance_new(store, module, &imports, &trap);
    wasm_module_delete(module);
    if (inst == NULL) {
        printf("error: failed to instantinate module\n");
        return 1;
    }

    // get the _start function
    own wasm_extern_vec_t externs;
    wasm_instance_exports(inst, &externs);
    wasm_func_t *start_func = wasm_extern_as_func(externs.data[start_func_index]);

    //spawn the profiler
    pid_t this_pid = getpid();
    pid_t new_pid = fork();
    if (new_pid == 0) {
        // construct argv
        int new_argc = argc - 2;
        char pid[12];
        sprintf(pid, "%d", this_pid);
        char ** new_argv = malloc(sizeof(char*)*(new_argc+1));
        for (int i=0; i<new_argc; ++i) {
            if (strncmp(argv[i+2], "PID", 3) == 0) {
                new_argv[i] = pid;
            } else {
                new_argv[i] = argv[i+2];
            }
        }
        new_argv[new_argc] = NULL;
        // exec the profiler
        execvp(new_argv[0], new_argv);
    } else if (new_pid < 0) {
        printf("failed to start the profiler\n");
        return 1;
    }

    //call _start
    wasm_val_vec_t args = WASM_EMPTY_VEC;
    wasm_val_vec_t results = WASM_EMPTY_VEC;
    wasm_func_call(start_func, &args, &results);

    // stop profiling after the _start() returns
    kill(new_pid, SIGINT);

    // clean up
    wasm_extern_vec_delete(&externs);
    wasm_instance_delete(inst);

    wasm_store_delete(store);
    wasm_engine_delete(engine);
    return 0;
}

void print_usage() {
    printf(
        "Usage: wa-profiler <module> <profiler> <args>\n"
        "\tmodule: the file name of the module to be profiled\n"
        "\tprofiler: the profiler to invoke, for example, perf\n"
        "\targs: the arguments passed to the profiler, \"PID\" will be substituted with the PID of wa-profiler itself, where the module is run\n"
        "example:\n"
        "\twa-profiler fft.wasm perf stat -p PID"
    );
}
