# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Include build functions from Pico SDK
#include(ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)


# Set name of project (as PROJECT_NAME) and C/C   standards
project(hyperspace C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# libfixmath source files
set(LIBFIXMATH_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/libfixmath/fix16.c
    ${CMAKE_CURRENT_LIST_DIR}/libfixmath/fix16_exp.c
    ${CMAKE_CURRENT_LIST_DIR}/libfixmath/fix16_sqrt.c
    ${CMAKE_CURRENT_LIST_DIR}/libfixmath/fix16_str.c
    ${CMAKE_CURRENT_LIST_DIR}/libfixmath/fix16_trig.c
    ${CMAKE_CURRENT_LIST_DIR}/libfixmath/fract32.c
    ${CMAKE_CURRENT_LIST_DIR}/libfixmath/uint32.c
)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

include(${CMAKE_CURRENT_LIST_DIR}/picosystem_hardware/picosystem_hardware.cmake REQUIRED)


# Tell CMake where to find the executable source file
#add_executable(${PROJECT_NAME} 
#  main.c
#)
# Create map/bin/hex/uf2 files
#pico_add_extra_outputs(${PROJECT_NAME})

function(picosystem_hardware_executable NAME SOURCES)

  add_executable(
    ${NAME}
    ${SOURCES}
    ${ARGN}
  )

  # Pull in pico libraries that we need
  target_link_libraries(${NAME} picosystem_hardware)
  # Link to pico_stdlib (gpio, time, etc. functions)
  target_link_libraries(${NAME} pico_stdlib)
  # Link to hardware_flash and hardware_sync for persistent storage
  target_link_libraries(${NAME} hardware_flash hardware_sync)

  # Enable usb output, disable uart output
  pico_enable_stdio_usb(${NAME} 0)
  pico_enable_stdio_uart(${NAME} 1)

  # create map/bin/hex file etc.
  pico_add_extra_outputs(${NAME})

  pico_set_linker_script(${NAME} ${picosystem_hardware_LINKER_SCRIPT})

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2 DESTINATION .)
endfunction()

# Add libfixmath as a library
add_library(libfixmath STATIC ${LIBFIXMATH_SOURCES})
target_include_directories(libfixmath PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# Define FIXMATH_NO_OVERFLOW for better performance
target_compile_definitions(libfixmath PUBLIC FIXMATH_NO_OVERFLOW)

picosystem_hardware_executable(
    ${PROJECT_NAME}
    main.c
)

# Link libfixmath to the main executable
target_link_libraries(${PROJECT_NAME} libfixmath)

# Include directories for the main executable
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})



# Enable usb output, disable uart output
# Same settings exists in picosystem_hardware.cmake
#pico_enable_stdio_usb(${PROJECT_NAME} 0)
#pico_enable_stdio_uart(${PROJECT_NAME} 1)