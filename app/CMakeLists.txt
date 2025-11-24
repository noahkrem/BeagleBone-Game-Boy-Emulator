## Build the app, using the HAL

## Build a core library from app sources (so tests can link against it)

set(GBE_CORE_SOURCES
      src/cpu.c
      src/gpu.c
      src/input.c
      src/rom.c
      src/memory.c
      src/registers.c
)

add_library(gbe_core STATIC ${GBE_CORE_SOURCES})
target_include_directories(gbe_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link HAL to the core library
target_link_libraries(gbe_core PUBLIC hal)

## Build the final executable which links against the core library
add_executable(gbe src/main.c)
target_link_libraries(gbe PRIVATE gbe_core SDL3::SDL3)

# Copy executable to final location (change the destination path as needed)
add_custom_command(TARGET gbe POST_BUILD
   COMMAND "${CMAKE_COMMAND}" -E copy
      "$<TARGET_FILE:gbe>"
      "~/ensc-351/public/proj/gbe"  # NOTE: CHANGE THIS TO YOUR NFS DIRECTORY
   COMMENT "Copying ARM executable to public NFS directory")
