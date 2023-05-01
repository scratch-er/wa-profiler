#include <stdio.h>
#include <stdlib.h>
#include <wasm3.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <m3_api_libc.h>
#include <m3_env.h>
#include <m3_api_wasi.h>

void print_usage();
int usleep (uint32_t useconds);
typedef int32_t pid_t;

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    // new runtime environment
    IM3Environment env = m3_NewEnvironment();
    IM3Runtime runtime = m3_NewRuntime (env, 65536, NULL);
    if (runtime == NULL) {
        printf("Error initializing runtime\n");
        return 1;
    }

    // load wasm file
    FILE* file = fopen(argv[1], "rb");
    if (!file) {
        printf("> Error opening file!\n");
        return 1;
    }
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    uint8_t *binary = malloc(file_size);
    if (fread(binary, file_size, 1, file) != 1) {
        printf("> Error loading file!\n");
        return 1;
    }
    fclose(file);

    // parsing module
    M3Result result = m3Err_none;
    IM3Module module = NULL;
    result = m3_ParseModule(env, &module, binary, file_size);
    if (result != NULL) {
        printf("Error parsing module\n");
        return 1;
    }
    result = m3_LoadModule (runtime, module);
    if (result != NULL) {
        printf("Error loading module\n");
        return 1;
    }
    m3_SetModuleName(module, argv[1]);

    // linking
    result = m3_LinkSpecTest(module);
    if (result != NULL) {
        printf("Error linking module\n");
        return 1;
    }
    result = m3_LinkLibC(module);
    if (result != NULL) {
        printf("Error linking module\n");
        return 1;
    }
    result = m3_LinkWASI(module);
    if (result != NULL) {
        printf("Error linking module\n");
        return 1;
    }

    // find _start()
    IM3Function func;
    result = m3_FindFunction (&func, runtime, "_start");
    if (result != NULL) {
        printf("_start() not found\n");
        return 1;
    }

    m3_wasi_context_t* wasi_ctx = m3_GetWasiContext();
    wasi_ctx->argc = 1;
    wasi_ctx->argv = argv + 1;

    // spawn the profiler
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

    // sleep to wait for the profiler to start
    usleep(100000);

    // call _start()
    m3_CallArgv(func, 0, NULL);

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
