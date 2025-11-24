# tests/CMakeLists.txt

# Core test executables (CPU + bootloader don’t need SDL)
add_executable(cpu_test cpu_test.c)
target_link_libraries(cpu_test PRIVATE gbe_core)

add_executable(bootloader_test bootloader_test.c)
target_link_libraries(bootloader_test PRIVATE gbe_core)

# GPU test (optionally uses SDL3 for graphics)
add_executable(gpu_test gpu_test.c)
target_link_libraries(gpu_test PRIVATE gbe_core)

# Try to use SDL3 if it was found at the top level.
# The top-level CMakeLists.txt should contain:
#   find_package(SDL3 REQUIRED CONFIG
#       PATHS /opt/aarch64-sdl2/lib/cmake/SDL3
#       NO_DEFAULT_PATH
#   )
if(TARGET SDL3::SDL3)
    message(STATUS "Linking gpu_test with SDL3")
    target_link_libraries(gpu_test PRIVATE SDL3::SDL3)
else()
    message(WARNING "SDL3 not available. gpu_test will be built without graphics or may not function fully.")
    set_target_properties(gpu_test PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
    )
endif()

# If an NFS directory is configured in the app module, copy the gpu_test
# executable there as well (mirrors the behavior used for `gbe`). This step
# runs only when the gpu_test target exists (SDL2 found) and `GBE_NFS_DIR` is
# set (it defaults to the value from app/CMakeLists.txt but can be overridden
# with -DGBE_NFS_DIR=/path/to/dir when invoking CMake).
if(TARGET gpu_test AND GBE_NFS_DIR)
    add_custom_command(TARGET gpu_test POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:gpu_test>"
            "${GBE_NFS_DIR}"
        COMMENT "Copying gpu_test executable to NFS directory: ${GBE_NFS_DIR}")
endif()


## Copy cpu_test and bootloader_test executables to the same NFS dir when available
if(TARGET cpu_test AND GBE_NFS_DIR)
    add_custom_command(TARGET cpu_test POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:cpu_test>"
            "${GBE_NFS_DIR}"
        COMMENT "Copying cpu_test executable to NFS directory: ${GBE_NFS_DIR}")
endif()

if(TARGET bootloader_test AND GBE_NFS_DIR)
    add_custom_command(TARGET bootloader_test POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:bootloader_test>"
            "${GBE_NFS_DIR}"
        COMMENT "Copying bootloader_test executable to NFS directory: ${GBE_NFS_DIR}")
endif()

# Register tests with CTest
add_test(
    NAME cpu_unit_tests
    COMMAND cpu_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME bootloader_unit_tests
    COMMAND bootloader_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(
    NAME gpu_unit_tests
    COMMAND gpu_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Ensure PATH contains the test binaries directory
set_tests_properties(cpu_unit_tests
    PROPERTIES
    ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}"
)

set_tests_properties(bootloader_unit_tests
    PROPERTIES
    ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}"
)

set_tests_properties(gpu_unit_tests
    PROPERTIES
    ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}"
)
