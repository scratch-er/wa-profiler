# wa-profiler

wa-profiler is a simple tool to profile WebAssembly  modules. It aims to eliminate instantiating overhead of the runtime when collecting performace data. wa-profile do not do any profiling itself. Instead, it invokes an external profiler, such as perf.

`wa-profiler.c` is a general profiler supporting most major wasm runtimes. It is based on the standard C API of WASM. However, it does not support wasi since there's not a standard C API for wasi yet.

`wasmtime-profiler.c` profiles wasmtime.

`wamr-profiler.c` profiles wasm-micro-runtime.

`wasm3-profiler.c` profiles wasm3.

## Usage

```shell
wa-profiler <module> <profiler> <args>
```

module: the file name of the module to be profiled

profiler: the profiler to invoke, for example, perf 

args: the arguments passed to the profiler, "PID" will be substituted with the PID of wa-profiler itself, where the module is run

for example:

```shell
wa-profiler my-wa-module perf stat -p PID
```

## How it works

wa-profiler is based on the C API of the wasm runtime. It loads and instantiate the module, and then starts the profiler. So instantiating overhead is eliminated. After the profiler is started, it `usleep()` for a short while (to wait for the profiler to startup, and `usleep()` causes little overhead) and call the default export of the module. 

## Building

`wa-profiler.c` needs to be compiled with `-lwasmtime`, `-liwasm` or `-lwasmer`, to profile wasmtime, wasm-micro-runtime or wasmer respectively. Note that the standard WASM API implement of wasm-micro-runtime is buggy so expect unexpected problems!

`wasmtime-profiler.c` needs to be compiled with `-lwasmtime`.

`wamr-profiler.c` is an application embeding wasm-micro-runtime. Please refer to  [wasm-micro-runtime/embed_wamr.md at main · bytecodealliance/wasm-micro-runtime · GitHub](https://github.com/bytecodealliance/wasm-micro-runtime/blob/main/doc/embed_wamr.md) or simply replace the `main.c` of wasm-micro-rumtime with `wamr-profiler.c`.

`wasm3-profiler.c`is an application embeding wasm3. Please refer to [GitHub - wasm3/wasm3: 🚀 A fast WebAssembly interpreter and the most universal WASM runtime](https://github.com/wasm3/wasm3) or simply replace the`main.c`of wasm3 with `wasm3-profiler.c`.
