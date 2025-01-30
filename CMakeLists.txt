###############################################################################
# Main user settings
###############################################################################
# Project metadata
set(PROJECT_NAME        "gateway")
set(PROJECT_DESCRIPTION "6 Channel ETH <> RS485 Gateway")
set(PROJECT_VERSION     "0.1")
set(PROJECT_URL         "https://github.com/0x3333/gateway-6ch")

# The target RP2040 board.
set(PICO_BOARD pico CACHE STRING "Board type")

set(LIBS_PATH deps)

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/${LIBS_PATH}/pico-sdk)
set(PICO_EXTRAS_PATH ${CMAKE_CURRENT_LIST_DIR}/${LIBS_PATH}/pico-extras)
set(NANOMODBUS_PATH ${CMAKE_CURRENT_LIST_DIR}/${LIBS_PATH}/nanoMODBUS)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG_BUILD)
endif()

# If we want to print CPU/HEAP usage in the console
add_definitions(-DSHOW_CPU_USAGE=0)
add_definitions(-DSHOW_HEAP_USAGE=0)

# nanoMODBUS
add_definitions(-DNMBS_SERVER_DISABLED=1)
# add_definitions(-DNMBS_DEBUG=1)

# Declare the source files.
set(PROJECT_SRC_FILES
    src/main.c
    src/bus.c
    src/crc.c
    src/host.c
    src/led.c
    src/min_port.c
    src/messages.c
    src/res_usage.c
    src/uart.c
    src/ring_buffer.c
    ${NANOMODBUS_PATH}/nanomodbus.c
)

# Declare the include paths.
set(PROJECT_INCLUDE_PATHS
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/inc
    ${NANOMODBUS_PATH}
)

# Declare the libraries that will be linked in.
set(PROJECT_LINK_LIBRARIES
    pico_stdlib                   # Pico standard library, includes:
                                  #   - hardware_uart
                                  #   - hardware_gpio
                                  #   - pico_binary_info
                                  #   - pico_runtime
                                  #   - pico_platform
                                  #   - pico_printf
                                  #   - pico_stdio
                                  #   - pico_standard_link
                                  #   - pico_util
    #hardware_adc                 # Pico ADC hardware.
    #hardware_base                # Pico low level MMIO.
    #hardware_claim               # Pico hardware resource management.
    #hardware_clocks              # Pico hardware clocks.
    #hardware_divider             # Pico hardware divider.
    #hardware_dma                 # Pico DMA hardware.
    #hardware_exception           # Pico hardware exception handlers.
    #hardware_flash               # Pico flash hardware.
    #hardware_gpio                # Pico GPIO hardware. Default.
    #hardware_i2c                 # Pico I2C hardware.
    #hardware_interp              # Pico hardware interpolator.
    #hardware_irq                 # Pico IRQ hardware.
    #hardware_pll                 # Pico PLL hardware.
    #hardware_pwm                 # Pico PWM hardware.
    #hardware_resets              # Pico hardware resets.
    #hardware_rtc                 # Pico RTC hardware.
    #hardware_spi                 # Pico SPI hardware.
    #hardware_timer               # Pico hardware timer.
    #hardware_uart                # Pico UART hardware. Default.
    #hardware_vreg                # Pico internal voltage regulator.
    #hardware_watchdog            # Pico hardware watchdog.
    #pico_async_context           # Pico high level threading.
    #pico_multicore               # Pico high level multicore support.
    #pico_i2c_slave               # Pico high level i2c slave support.
    #pico_rand                    # Pico high level rng.
    #pico_sync                    # Pico threading primitives.
    #pico_time                    # Pico timestamps and time based callbacks.
    #pico_unique_id               # Pico device ID.
    #pico_util                    # Pico utilities and data structures.
    #tinyusb_device tinyusb_board # Pico USB support.
    FreeRTOS-Kernel               # FreeRTOS kernel.
    #FreeRTOS-Kernel-Static       # FreeRTOS kernel supporting static allocation.
    #FreeRTOS-Kernel-Heap1        # FreeRTOS heap allocator level 1.
    #FreeRTOS-Kernel-Heap2        # FreeRTOS heap allocator level 2.
    #FreeRTOS-Kernel-Heap3        # FreeRTOS heap allocator level 3.
    FreeRTOS-Kernel-Heap4         # FreeRTOS heap allocator level 4. The default.
    #FreeRTOS-Kernel-Heap5        # FreeRTOS heap allocator level 5.
    min                           # Min Protocol.
)

# PIO settings.
set(PROJECT_USES_PIO 1)
set(PROJECT_PIO_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/pio/uart_tx.pio
    ${CMAKE_CURRENT_LIST_DIR}/src/pio/uart_rx.pio
)

# Miscellaneous options.
set(PICO_CXX_ENABLE_EXCEPTIONS 0) # Enable C++ exceptions.
set(PICO_CXX_ENABLE_RTTI       0) # Enable C++ runtime type information.
###############################################################################

# CMAKE requirements
cmake_minimum_required(VERSION 3.13)

# Find dependencies
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/${LIBS_PATH}/pico-sdk/external/")
include(pico_sdk_import)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/${LIBS_PATH}/pico-extras/external/")
include(pico_extras_import)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/${LIBS_PATH}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/")
include(FreeRTOS_Kernel_import)

# Set up our main project.
project(
    ${PROJECT_NAME}
    LANGUAGES C CXX ASM
    DESCRIPTION ${PROJECT_DESCRIPTION}
)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the pico SDK.
pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/${LIBS_PATH}/min)

# Add the executable for our project.
add_executable(${PROJECT_NAME} ${PROJECT_SRC_FILES})

# Pico specific metadata for the executable.
pico_set_program_name(${PROJECT_NAME}        "${PROJECT_NAME}")
pico_set_program_description(${PROJECT_NAME} "${PROJECT_DESCRIPTION}")
pico_set_program_url(${PROJECT_NAME}         "${PROJECT_URL}")
pico_set_program_version(${PROJECT_NAME}     "${PROJECT_VERSION}")

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# Process PIO files if required.
if(${PROJECT_USES_PIO})
    list(APPEND PROJECT_LINK_LIBRARIES hardware_pio)
    foreach(PIO ${PROJECT_PIO_SOURCES})
        pico_generate_pio_header(${PROJECT_NAME} ${PIO})
        message(${PIO})
    endforeach()
endif()


# Libraries, includes and defines.
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_INCLUDE_PATHS})
target_link_libraries(${PROJECT_NAME} ${PROJECT_LINK_LIBRARIES})

# Enable compiler insights
set_source_files_properties(${PROJECT_SRC_FILES} PROPERTIES COMPILE_FLAGS "-Wextra -Wpedantic -Wall")
set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")

# Project defines.
target_compile_definitions(${PROJECT_NAME} PUBLIC
    # Use ESP UART as alternative to debugging
    PICO_DEFAULT_UART=1
    PICO_DEFAULT_UART_BAUD_RATE=230400
    PICO_DEFAULT_UART_TX_PIN=4
    PICO_DEFAULT_UART_RX_PIN=5
)

# Libs defines.
target_compile_definitions(min PUBLIC
    MAX_PAYLOAD=32
    NO_TRANSPORT_PROTOCOL=1
)

# Tell the pico SDK to also output bin/hex/uf2 files.
pico_add_extra_outputs(${PROJECT_NAME})

# Custom upload target for Raspberry Pi Pico using picotool
add_custom_target(upload
    COMMAND picotool load ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.elf -fx
    DEPENDS ${PROJECT_NAME}
    COMMENT "Uploading ${PROJECT_NAME}.elf to Raspberry Pi Pico"
)
