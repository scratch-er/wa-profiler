#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wasm.h>
#include <wasi.h>
#include <wasmtime.h>
#include <unistd.h>
#include <signal.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap);
void print_usage();

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    // Set up our context
    wasm_engine_t *engine = wasm_engine_new();
    assert(engine != NULL);
    wasmtime_store_t *store = wasmtime_store_new(engine, NULL, NULL);
    assert(store != NULL);
    wasmtime_context_t *context = wasmtime_store_context(store);

    // Create a linker with WASI functions defined
    wasmtime_linker_t *linker = wasmtime_linker_new(engine);
    wasmtime_error_t *error = wasmtime_linker_define_wasi(linker);
    if (error != NULL) {
        exit_with_error("failed to link wasi", error, NULL);
    }

    wasm_byte_vec_t wasm;
    // Load our input file to parse it next
    FILE* file = fopen(argv[1], "rb");
    if (!file) {
        printf("> Error loading file!\n");
        exit(1);
    }
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    wasm_byte_vec_new_uninitialized(&wasm, file_size);
    fseek(file, 0L, SEEK_SET);
    if (fread(wasm.data, file_size, 1, file) != 1) {
        printf("> Error loading module!\n");
        exit(1);
    }
    fclose(file);

    // Compile our modules
    wasmtime_module_t *module = NULL;
    error = wasmtime_module_new(engine, (uint8_t*)wasm.data, wasm.size, &module);
    if (!module) {
        exit_with_error("failed to compile module", error, NULL);
    }
    wasm_byte_vec_delete(&wasm);

    // Instantiate wasi
    wasi_config_t *wasi_config = wasi_config_new();
    assert(wasi_config);
    wasi_config_inherit_argv(wasi_config);
    wasi_config_inherit_env(wasi_config);
    wasi_config_inherit_stdin(wasi_config);
    wasi_config_inherit_stdout(wasi_config);
    wasi_config_inherit_stderr(wasi_config);
    wasm_trap_t *trap = NULL;
    error = wasmtime_context_set_wasi(context, wasi_config);
    if (error != NULL) {
        exit_with_error("failed to instantiate WASI", error, NULL);
    }

    // Instantiate the module
    error = wasmtime_linker_module(linker, context, "", 0, module);
    if (error != NULL) {
        exit_with_error("failed to instantiate module", error, NULL);
    }

    // Get the entry function
    wasmtime_func_t func;
    error = wasmtime_linker_get_default(linker, context, "", 0, &func);
    if (error != NULL) {
        exit_with_error("failed to locate default export for module", error, NULL);
    }

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

    // Run the module
    error = wasmtime_func_call(context, &func, NULL, 0, NULL, 0, &trap);
    if (error != NULL || trap != NULL) {
        exit_with_error("error calling default export", error, trap);
    }

    // stop profiling after the _start() returns
    // kill(new_pid, SIGINT);

    // Clean up after ourselves at this point
    wasmtime_module_delete(module);
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);
    return 0;
}

static void exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap) {
    fprintf(stderr, "error: %s\n", message);
    wasm_byte_vec_t error_message;
    if (error != NULL) {
        wasmtime_error_message(error, &error_message);
        wasmtime_error_delete(error);
    } else {
        wasm_trap_message(trap, &error_message);
        wasm_trap_delete(trap);
    }
    fprintf(stderr, "%.*s\n", (int) error_message.size, error_message.data);
    wasm_byte_vec_delete(&error_message);
    exit(1);
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
