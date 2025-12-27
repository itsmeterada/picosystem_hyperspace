add_library(picosystem_hardware INTERFACE)

pico_generate_pio_header(picosystem_hardware ${CMAKE_CURRENT_LIST_DIR}/screen.pio)
pico_generate_pio_header(picosystem_hardware ${CMAKE_CURRENT_LIST_DIR}/screen_double.pio)

target_sources(picosystem_hardware INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/picosystem_hardware.c
  ${CMAKE_CURRENT_LIST_DIR}/picosystem_draw.c
)

set(picosystem_hardware_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/memmap_picosystem.ld)

target_include_directories(picosystem_hardware INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(picosystem_hardware INTERFACE pico_stdlib hardware_pio hardware_spi hardware_pwm hardware_dma hardware_irq hardware_adc hardware_interp)

# function(picosystem_hardware_executable NAME SOURCES)

#   add_executable(
#     ${NAME}
#     ${SOURCES}
#     ${ARGN}
#   )

#   # Pull in pico libraries that we need
#   target_link_libraries(${NAME} picosystem_hardware)

#   # Enable usb output, disable uart output
#   pico_enable_stdio_usb(${NAME} 0)
#   pico_enable_stdio_uart(${NAME} 1)
  
#   # create map/bin/hex file etc.
#   pico_add_extra_outputs(${NAME})

#   pico_set_linker_script(${NAME} ${picosystem_hardware_LINKER_SCRIPT})

#   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.uf2 DESTINATION .)
# endfunction()

function(pixel_double NAME)
  target_compile_options(${NAME} PRIVATE -DPIXEL_DOUBLE)
endfunction()

function(no_overclock NAME)
  target_compile_options(${NAME} PRIVATE -DNO_OVERCLOCK)
endfunction()
