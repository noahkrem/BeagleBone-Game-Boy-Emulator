# BeagleBone Game Boy Emulator

This repository contains a small Game Boy emulator targeted to run on BeagleBone (aarch64) as well as on the host machine for development and testing.

At minimum this README provides a short introduction, the project layout, prerequisites, and instructions to build both natively and cross-compile for aarch64 using the included CMake toolchain file.

## Project overview

- Language: C
- Build system: CMake
- Target: an executable emulator (built under `build/app/gbe`)

The code is organized under `app/` (sources and headers) and platform HAL code under `hal/`.

## Prerequisites

- CMake (>= 3.18 recommended)
- A C compiler (gcc/clang) for native builds
- For cross-compilation to aarch64 you will need an aarch64 cross-toolchain such as `aarch64-linux-gnu-gcc` and `aarch64-linux-gnu-g++`. On Debian/Ubuntu you can install:

```bash
sudo apt update
sudo apt install cmake build-essential gcc g++
# For cross compiling to aarch64
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

There is a toolchain file at `cmake/aarch64-toolchain.cmake` included in this repository. You can edit it if your toolchain is in a non-standard path.

## SDL3 Setup for GPU Tests

The GPU test (`gpu_test`) requires SDL3 for graphics display. Follow these steps to set up SDL3:

### Cross-compile SDL3 on Host (Debian VM)

1. **Install SDL3 build dependencies on host:**

```bash
sudo apt-get update
sudo apt-get install git cmake make pkg-config \
    libasound2-dev libx11-dev libxext-dev libxrandr-dev \
    libxi-dev libxcursor-dev libxinerama-dev libxss-dev \
    libglu1-mesa-dev libwayland-dev libxkbcommon-dev \
    libdrm-dev libgbm-dev libudev-dev
```

2. **Clone and build SDL3 for aarch64:**

```bash
cd ~
mkdir -p src && cd src
git clone https://github.com/libsdl-org/SDL.git
cd SDL
mkdir build-aarch64 && cd build-aarch64

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
  -DCMAKE_INSTALL_PREFIX=/opt/aarch64-sdl2

cmake --build . --parallel 2
sudo cmake --install .
```

### Install SDL3 on BeagleY-AI Target

Since cross-compiled SDL3 may lack proper video driver support, build SDL3 natively on the BeagleY-AI:

1. **SSH into BeagleY-AI and install dependencies:**

```bash
ssh debian@beagley-ai.local

sudo apt-get update
sudo apt-get install git cmake build-essential \
    libx11-dev libxext-dev libxrandr-dev libxi-dev \
    libxcursor-dev libxinerama-dev libxss-dev libxtst-dev \
    libxxf86vm-dev libxfixes-dev \
    libwayland-dev libxkbcommon-dev \
    libdrm-dev libgbm-dev libudev-dev \
    libasound2-dev libpulse-dev libjack-jackd2-dev \
    libsamplerate0-dev libsndio-dev \
    libdbus-1-dev libibus-1.0-dev \
    libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev
```

2. **Build and install SDL3 on BeagleY-AI:**

```bash
cd ~
git clone https://github.com/libsdl-org/SDL.git
cd SDL
mkdir build && cd build

cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build . --parallel 2
sudo cmake --install .
sudo ldconfig
```

3. **Verify SDL3 installation:**

```bash
ldconfig -p | grep SDL3
# Should show: libSDL3.so.0 => /usr/local/lib/libSDL3.so.0
```

## Build (native)

Build a native (host) executable in a `build/` directory:

```bash
cmake -S . -B build
cmake --build build

# The emulator binary is typically placed at:
ls build/app/gbe
```

Run the emulator on your host as appropriate (may require additional runtime configuration or assets).

## Cross-compile for aarch64 (BeagleBone)

Use the included CMake toolchain file to produce an aarch64 binary. Example:

```bash
cmake -S . -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-toolchain.cmake
cmake --build build-aarch64

# Resulting binary:
ls build-aarch64/app/gbe
```

To compile the GPU test with SDL3:

```bash
cmake -S . -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-toolchain.cmake \
  -DCMAKE_PREFIX_PATH=/opt/aarch64-sdl2
cmake --build build-aarch64
```

### Important Notes!!
- *If you are using NFS to share the executable with your BeagleBone, you should change the commented line in /app/CMakeLists.txt to correspond to your own NFS directory location!*
  - E.g. change from `~/ensc-351/public/proj/gbe` to `~/ENSC351/public/finalproject/github` or whatever your filepath may be.
- If your cross-toolchain uses a different prefix or lives in a custom directory, either edit `cmake/aarch64-toolchain.cmake` or pass `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER` variables on the cmake command line.
- You may need to set `CMAKE_SYSROOT` if your toolchain expects a sysroot.

## Project layout

- `app/` - emulator sources and headers (CPU, GPU, input, memory, main)
- `hal/` - hardware abstraction layer (BeagleBone- and hardware-specific code)
- `tests/` - unit tests and test framework
- `cmake/` - (optional) toolchain files (e.g. `aarch64-toolchain.cmake`)
- `build/` - out-of-source build directory (generated)

## Running Tests

The project includes unit tests in the `tests/` directory. To build and run the tests:

```bash
# Configure and build the project with tests
cmake -S . -B build
cmake --build build
```

Individual test executables can be run directly:

```bash
# Run CPU tests specifically
./build/tests/cpu_test

# Run GPU test (requires SDL3 and display)
./build/tests/gpu_test
```

### Running GPU Test on BeagleY-AI

Copy the test executable and run it from the GUI terminal (not SSH):

```bash
# On host VM:
scp build-aarch64/tests/gpu_test debian@beagley-ai.local:~/

# On BeagleY-AI (from GUI terminal with HDMI display):
cd ~
./gpu_test
```

**Note:** Address sanitizer may report memory leaks from SDL3/X11 libraries. These are typically harmless library-level allocations. To suppress leak checking:

```bash
export ASAN_OPTIONS=detect_leaks=0
./gpu_test
```

## Troubleshooting

- If cmake can't find your cross-compiler, install or point to the correct compiler binary.
- If linking fails on cross-compile due to missing libraries, provide an appropriate `CMAKE_SYSROOT` or build missing libs for the target.
- If `gpu_test` reports "No available video device", ensure SDL3 was built with X11/Wayland support on the BeagleY-AI.
- If SDL3 shared library is not found at runtime, verify with `ldd ./gpu_test` and ensure `/usr/local/lib` is in your library path.

## Finer Points

- When using the header files in HAL, you'll need to:  
  `#include "hal/myfile.h"`  
  This extra "hal/..." helps distinguish the low-level access from the higher-level code.
- One only need to run the CMake build the first time the project loads, and each time the .h and .c file names change, or new ones are added, or ones are removed. This regenerates the `build/Makefile`. Otherwise, just run a normal build (ctrl+shift+B)
- If desired, one could provide an alternative implementation for the HAL modules that provides a software simulation of the hardware! This could be a useful idea if you have some complex hardware, or limited access to some hardware.
