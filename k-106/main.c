#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "bsp/board.h"

#include "tusb.h"
#include "mpc4822.h"
#include "core1.h"
#include "midi.h"
#include "midinote.h"

// test
const uint ledPin  = 16;    // GP16 (21)

int main()
{
    uint clk = 125000;//200000;
    bool clkset = set_sys_clock_khz(clk, true);
    stdio_init_all();
    board_init();    
    sleep_ms(500);
    printf("\n\n[start init] ==========%dMHz==========\n", clk/1000);    
    core1Init();
    dacInit();
    midiInit(true, true, true);    
    midiNoteInit();
    //frequencyCounterInit();    

    gpio_init(ledPin); 
    gpio_set_dir(ledPin, GPIO_OUT);

    multicore_launch_core1(core1Entry);

    printf("\n[done init]  ==========================\n");    
    
    // test
    gpio_put(ledPin, true);
    
      
    uint32_t freqOld = 0;
    uint32_t startTimeout = 0;
    bool started = false;
    while (true) 
    {                            
        midiProcessTask();
        // WARNING: ...tasks() need a time! Be faster.

        //ledBlinkingProcessTask();
        midiNoteTask();
    }
    printf("\n\n[stop!]  ==========================\n");   
    gpio_put(ledPin, 0);
    return 0;
}




