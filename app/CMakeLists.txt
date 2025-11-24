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

# NFS destination for copied executables. Make this a cache variable so it can
# be overridden from the command line or the parent project. Example usage:
#  -DGBE_NFS_DIR=/home/user/public/proj
set(GBE_NFS_DIR "~/ensc351/public/proj" CACHE PATH "NFS directory to copy built executables to (optional)")

if(GBE_NFS_DIR)
   add_custom_command(TARGET gbe POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy
         "$<TARGET_FILE:gbe>"
         "${GBE_NFS_DIR}"
      COMMENT "Copying gbe executable to NFS directory: ${GBE_NFS_DIR}")
endif()

# Also copy the ROM files directory to the NFS location so the device has access
# to test ROMs. This creates `${GBE_NFS_DIR}/rom` and copies the contents of the
# repository `rom/` directory into it. Use an absolute path for `GBE_NFS_DIR`.
if(GBE_NFS_DIR)
   add_custom_command(TARGET gbe POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${GBE_NFS_DIR}/rom"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/rom" "${GBE_NFS_DIR}"
      COMMENT "Copying rom/ folder to NFS directory: ${GBE_NFS_DIR}/rom")
endif()
