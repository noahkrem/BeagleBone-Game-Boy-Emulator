# CMake Build Configuration for root of project
cmake_minimum_required(VERSION 3.18)
project(game_boy_emulator VERSION 1.0 DESCRIPTION "BBY-AI GB Emulator" LANGUAGES C)

# Export compile commands so IDEs (IntelliSense/clangd) can pick up include flags
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Compiler options (inherited by sub-folders)
set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Werror -Wpedantic -Wextra)
add_compile_options(-fdiagnostics-color)

# Enable address sanitizer
# (Comment this out to make your code faster)
# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

# Enable PThread library for linking
add_compile_options(-pthread)
add_link_options(-pthread)

# Enable testing
include(CTest)
enable_testing()

# SDL3
if(CMAKE_CROSSCOMPILING)
    # Cross build: you called cmake with -DCMAKE_TOOLCHAIN_FILE=...
    message(STATUS "Configuring for cross-compilation (aarch64)")
    find_package(SDL3 REQUIRED CONFIG
        PATHS /opt/aarch64-sdl2/lib/cmake/SDL3
        NO_DEFAULT_PATH
    )
else()
    # Native build on Debian VM
    message(STATUS "Configuring for native build")
    find_package(SDL3 REQUIRED CONFIG)
endif()

# What folders to build
add_subdirectory(hal)
add_subdirectory(app)
add_subdirectory(tests)
