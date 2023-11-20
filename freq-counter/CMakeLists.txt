#mkdir build
#cd build
#cmake ../CMakeLists.txt
#make -j4

#MSYS2
#picotool load -v -x testpico.uf2 -f

cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(testpico C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD pico)
pico_sdk_init()

add_executable(${PROJECT_NAME} 
    main.c
    usb_descriptors.c
    mpc4822.c
    frequencyCounter.c
    core1.c
    midi.c
    midinote.c
    crc8x.c
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/pps.pio)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/counter.pio)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/pico-dco.pio)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    pico_stdlib 
    pico_multicore
    hardware_pio
    hardware_dma
    hardware_spi
    hardware_pwm
    hardware_adc
    tinyusb_device    
    tinyusb_board
)

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)


pico_add_extra_outputs(${PROJECT_NAME})