cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")
#set(CYW43_ENABLE_BLUETOOTH 1)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Final_project C CXX ASM)
# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Final_project)

target_sources(Final_project PRIVATE
    src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/drivers/gamepad_HID.c
    #${CMAKE_CURRENT_LIST_DIR}/drivers/hog_keyboard_demo.c
)

#pico_btstack_make_gatt_header(Final_project PRIVATE
#    "${CMAKE_CURRENT_LIST_DIR}/drivers/hog_keyboard_demo.gatt"
#)

pico_set_program_name(Final_project "Final_project")
pico_set_program_version(Final_project "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Final_project 1)
pico_enable_stdio_usb(Final_project 1)

# Add the standard include files to the build
target_include_directories(Final_project PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/drivers
)

# Add the standard library to the build
target_link_libraries(Final_project
        pico_stdlib
        pico_btstack_ble          # For BLE
        pico_btstack_cyw43        # Required for CYW43 Bluetooth integration
        pico_cyw43_arch_none      # Required for Pico W architecture
)

target_compile_definitions(Final_project PRIVATE
    CYW43_ENABLE_BLUETOOTH=1
)

# Add any user requested libraries
target_link_libraries(Final_project 
        pico_cyw43_arch_none
)

pico_add_extra_outputs(Final_project)

