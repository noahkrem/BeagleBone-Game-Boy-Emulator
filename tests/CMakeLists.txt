add_executable(cpu_test cpu_test.c)

# Link against the core library. gbe_core exposes its include directories as PUBLIC,
# so cpu_test will automatically get the include path for headers like cpu.h
target_link_libraries(cpu_test PRIVATE gbe_core)

# Add the test to CTest
add_test(
    NAME cpu_unit_tests
    COMMAND cpu_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Set test properties
set_tests_properties(cpu_unit_tests
    PROPERTIES
    ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}"
)