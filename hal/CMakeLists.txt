# CMakeLists.txt for HAL
#   Build a library (`hal`) which exposes the header files as "hal/*.h"
#   Use headers as: #include "joystick.h" or #include "buttons.h"
#   (or "hal/..." depending on how you place them under include/)

# Collect all C sources in hal/src (including joystick.c and buttons.c)
file(GLOB MY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Build static HAL library
add_library(hal STATIC ${MY_SOURCES})

# Public include directory for HAL headers
# If your headers live in hal/include/, this makes them visible as:
#   #include "joystick.h"
#   #include "buttons.h"
target_include_directories(hal
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link HAL against libgpiod (for buttons.c)
find_library(GPIOD_LIB gpiod)
if(NOT GPIOD_LIB)
    message(FATAL_ERROR "libgpiod not found – required for buttons.c")
endif()

target_link_libraries(hal
    PUBLIC
        ${GPIOD_LIB}
)
