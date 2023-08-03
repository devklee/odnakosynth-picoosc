cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(testspi C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD pico)
pico_sdk_init()

add_executable(${PROJECT_NAME} 
    main.c
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    pico_stdlib 
    hardware_spi    
)

pico_add_extra_outputs(${PROJECT_NAME})