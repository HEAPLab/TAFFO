<img src="_logo/TAFFO-logo-black.png" alt="TAFFO" width=30%>

*(Tuning Assistant for Floating Point to Fixed Point Optimization)*

TAFFO is an autotuning framework which tries to replace floating point operations with fixed point operations as much as possible.

It is based on LLVM 10 and has been tested on Linux and macOS (any attempt to compile on Windows or WSL is at your own risk and peril).

## How to use TAFFO

Taffo currently ships as 5 LLVM plugins, each one of them containing one LLVM optimization or analysis pass:

 - TaffoInitializer (Initialization pass)
 - TaffoVRA (Value Range Analysis pass)
 - TaffoDTA (Data Type Allocation pass)
 - LLVMFloatToFixed (Conversion pass)
 - LLVMErrorPropagator (Error Propagation pass of the Feedback Estimator)

To execute TAFFO, the LLVM `opt` tool is used to load one pass at a time and run it on LLVM IR source or bitcode files.

As running `opt` manually once for each pass is cumbersome, the `test` directory contains two scripts that simplify the usage of TAFFO.

 - `setenv.sh`, given the install prefix of TAFFO, sets several environment variables containing the path where the TAFFO LLVM plugins are installed.
 - `magiclang2.sh` is a wrapper that can be substituted to `clang` in order to compile or link executables using TAFFO. It uses the environment variables declared by `setenv.sh`.

Thus, to use TAFFO it is encouraged to follow these steps:

### Step 1

Create a build directory, compile and install TAFFO.
The suggested install directory is `dist` in the `TAFFO` repository.
If you have a single system LLVM version installed you may omit setting the `LLVM_DIR` variable to the path of the LLVM distribution you want to use.

Note that at the moment TAFFO supports only LLVM 10 and that LLVM plugins compiled for a given major version of LLVM cannot be loaded by any other version.
If you are building LLVM from sources, you must configure it with `-DLLVM_BUILD_LLVM_DYLIB=ON` and `-DLLVM_LINK_LLVM_DYLIB=ON` for the TAFFO build to succeed.

```sh
$ cd /path/to/the/location/of/TAFFO
$ export LLVM_DIR=/usr/lib/llvm-10
$ mkdir build
$ cd build
$ cmake ..
$ make install DESTDIR=../dist
```

### Step 2

Use the `setenv.sh` script to set the environment variables used by TAFFO to the correct values.

```sh
$ cd ..
$ source ./test/setenv.sh ./dist
``` 

### Step 3

Modify the application to insert annotations on the appropriate variable declarations, then use `magiclang2.sh` to use TAFFO to compile your application.

```sh
<editor> program.c
[...]
./test/magiclang2.sh -O3 -o program-taffo program.c
```

See the annotation syntax documentation or the examples in `test/simple-test-cases` to get an idea on how to write annotations. You can also test TAFFO without adding annotations, which will produce the same results as using `clang` as a compiler/linker instead of `magiclang2.sh`.

Note that there is no `magiclang2++.sh`; C++ source files are autodetected by the file extension instead.
