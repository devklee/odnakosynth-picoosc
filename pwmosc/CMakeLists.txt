#mkdir build
#cd build
#cmake ..
#make -j4

cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(testpwm C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico)

pico_sdk_init()

add_executable(${PROJECT_NAME}
        main.c
        )


# pull in common dependencies and additional pwm hardware support
target_link_libraries(${PROJECT_NAME} pico_stdlib hardware_pwm)

pico_add_extra_outputs(${PROJECT_NAME})

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
