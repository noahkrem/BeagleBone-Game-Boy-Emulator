add_executable(cpu_test cpu_test.c)
target_link_libraries(cpu_test PRIVATE gbe_core)

add_executable(bootloader_test bootloader_test.c)
target_link_libraries(bootloader_test PRIVATE gbe_core)

add_executable(gpu_test gpu_test.c)
target_link_libraries(gpu_test PRIVATE gbe_core)

# Add the tests to CTest
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

find_package(SDL2 REQUIRED)
target_link_libraries(gpu_test PRIVATE SDL2::SDL2)
