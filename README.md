# BeagleBone Game Boy Emulator

This repository contains a small Game Boy emulator targeted to run on BeagleBone (aarch64) as well as on the host machine for development and testing.

At minimum this README provides a short introduction, the project layout, prerequisites, and instructions to build both natively and cross-compile for aarch64 using the included CMake toolchain file.

## Project overview

- Language: C
- Build system: CMake
- Target: an executable emulator (built under `build/app/gbe`)

The code is organized under `app/` (sources and headers) and platform HAL code under `hal/`.

## Prerequisites

- CMake (>= 3.10 recommended)
- A C compiler (gcc/clang) for native builds
- For cross-compilation to aarch64 you will need an aarch64 cross-toolchain such as `aarch64-linux-gnu-gcc` and `aarch64-linux-gnu-g++`. On Debian/Ubuntu you can install:

```bash
sudo apt update
sudo apt install cmake build-essential gcc g++
# For cross compiling to aarch64
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

There is a toolchain file at `cmake/aarch64-toolchain.cmake` included in this repository. You can edit it if your toolchain is in a non-standard path.

## Build (native)

Build a native (host) executable in a `build/` directory:

```bash
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc)

# The emulator binary is typically placed at:
ls build/app/gbe
```

Run the emulator on your host as appropriate (may require additional runtime configuration or assets).

## Cross-compile for aarch64 (BeagleBone)

Use the included CMake toolchain file to produce an aarch64 binary. Example:

```bash
mkdir -p build-aarch64
cmake -S . -B build-aarch64 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-toolchain.cmake
cmake --build build-aarch64 -- -j$(nproc)

# Resulting binary:
ls build-aarch64/app/gbe
```

### Important Notes!!
- *If you are using NFS to share the executable with your BeagleBone, you should change the commented line in /app/CMakeLists.txt to correspond to your own NFS directory location!*
  - E.g. change from `~/ensc-351/public/proj/gbe` to `~/ENSC351/public/finalproject/github` or whatever your filepath may be.
- If your cross-toolchain uses a different prefix or lives in a custom directory, either edit `cmake/aarch64-toolchain.cmake` or pass `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER` variables on the cmake command line.
- You may need to set `CMAKE_SYSROOT` if your toolchain expects a sysroot.

## Project layout

- `app/` - emulator sources and headers (CPU, GPU, input, memory, main)
- `hal/` - hardware abstraction layer (BeagleBone- and hardware-specific code)
- `cmake/` - (optional) toolchain files (e.g. `aarch64-toolchain.cmake`)
- `build/` - out-of-source build directory (generated)

## Troubleshooting

- If cmake can't find your cross-compiler, install or point to the correct compiler binary.
- If linking fails on cross-compile due to missing libraries, provide an appropriate `CMAKE_SYSROOT` or build missing libs for the target.

## Finer Points

- When using the header files in HAL, you'll need to:  
  `#include "hal/myfile.h`  
  This extra "hal/..." helps distinguish the low-level access from the higher-level code.
- One only need to run the CMake build the first time the project loads, and each time the .h and .c file names change, or new ones are added, or ones are removed. This regenerates the `build/Makefile`. Otherwise, just run a normal build (ctrl+shift+B)
- If desired, one could provide an alternative implementation for the HAL modules that provides a software simulation of the hardware! This could be a useful idea if you have some complex hardware, or limited access to some hardware.