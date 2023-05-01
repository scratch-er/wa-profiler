#include <wasm_export.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define ERR_BUFF_SIZE 128

void print_usage();

int main(int argc, char **argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }


    char error_buf[ERR_BUFF_SIZE];
    wasm_module_t module;
    wasm_module_inst_t module_inst;
    uint32_t stack_size = 64 * 1024, heap_size = 16 * 1024;


    /* initialize the wasm runtime by default configurations */
    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(RuntimeInitArgs));
    init_args.mem_alloc_type = Alloc_With_Allocator;
    init_args.mem_alloc_option.allocator.malloc_func = malloc;
    init_args.mem_alloc_option.allocator.realloc_func = realloc;
    init_args.mem_alloc_option.allocator.free_func = free;
    wasm_runtime_full_init(&init_args);

    /* read WASM file into a memory buffer */
    FILE* wasm_file = fopen(argv[1], "rb");
    if (wasm_file == NULL) {
        printf("failed to open wasm module file\n");
        return 1;
    }
    fseek(wasm_file, 0, SEEK_END);
    size_t file_size = ftell(wasm_file);
    fseek(wasm_file, 0, SEEK_SET);
    uint8_t *buffer = malloc(file_size);
    fread(buffer, file_size, 1, wasm_file);
    fclose(wasm_file);

    /* parse the WASM file from buffer and create a WASM module */
    module = wasm_runtime_load(buffer, file_size, error_buf, ERR_BUFF_SIZE);
    if (module == NULL) {
        printf("failed to load wasm module\n");
        return 1;
    }

    /* create an instance of the WASM module (WASM linear memory is ready) */
    module_inst = wasm_runtime_instantiate(module, stack_size, heap_size, error_buf, ERR_BUFF_SIZE);


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

    usleep(100000);

    wasm_application_execute_main(module_inst, 1, argv+1);

    return 0;
}

void print_usage() {
    printf(
        "Usage: wa-profiler <module1> <module2> ... -- <profiler> <args>\n"
        "\tmodule: the file name of the module to be profiled\n"
        "\tprofiler: the profiler to invoke, for example, perf\n"
        "\targs: the arguments passed to the profiler, \"PID\" will be substituted with the PID of wa-profiler itself, where the module is run\n"
        "example:\n"
        "\twa-profiler fft.wasm perf stat -p PID\n"
    );
}
