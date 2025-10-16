# Build the app, using the HAL

include_directories(include)
file(GLOB MY_SOURCES "src/*.c")
add_executable(gbe ${MY_SOURCES})

# Make use of the HAL library
target_link_libraries(gbe LINK_PRIVATE hal)

# Copy executable to final location (change `hello_world` to project name as needed)
add_custom_command(TARGET gbe POST_BUILD 
  COMMAND "${CMAKE_COMMAND}" -E copy 
     "$<TARGET_FILE:gbe>"
     "~/ensc-351/public/proj/gbe"  # NOTE: Change this to your NFS directory
  COMMENT "Copying ARM executable to public NFS directory")
