cmake_minimum_required(VERSION 3.23)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(CGA C CXX ASM)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_BUILD_TYPE "MinSizeRel")

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

#add_link_options(-flto -fwhole-program)
add_compile_options(
        #-flto -fwhole-program
        -ffunction-sections -fdata-sections -Ofast
) # size optimizations
add_executable(${PROJECT_NAME})


pico_define_boot_stage2(slower_boot ${PICO_DEFAULT_BOOT_STAGE2_FILE})
target_compile_definitions(slower_boot PRIVATE PICO_FLASH_SPI_CLKDIV=4)
pico_set_boot_stage2(${PROJECT_NAME} slower_boot)

# 16megs of flash on purple pico clones
target_compile_definitions(${PROJECT_NAME} PRIVATE
        PICO_FLASH_SIZE_BYTES=16777216
        #        PICO_PRINTF_ALWAYS_INCLUDED=0
        #        PICO_STACK_SIZE=1024
        #        PICO_HEAP_SIZE=1024
        PICO_PANIC_FUNCTION=
)

#pico_set_float_implementation(${PROJECT_NAME} none) # size optimizations
#pico_set_double_implementation(${PROJECT_NAME} none) # size optimizations
#pico_set_printf_implementation(${PROJECT_NAME} none)



target_sources(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/main.c
)

target_include_directories(${PROJECT_NAME} PUBLIC
)

target_link_libraries(${PROJECT_NAME} PUBLIC
        pico_runtime
        pico_stdio
        hardware_pwm
        hardware_pio
        -Wl,--wrap=atexit # size optimizations
)


pico_enable_stdio_usb(${PROJECT_NAME} 1)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(${PROJECT_NAME})
target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage --data-sections --function-sections)

