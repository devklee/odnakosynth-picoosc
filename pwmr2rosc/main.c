#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
const uint MAX_DAC_LEVEL = 16; // 4 bit DAC
const uint PIN_1 = 10; 
const uint PIN_2 = 11;
const uint PIN_3 = 12;
const uint PIN_4 = 13;
const uint PIN_LOG = 14;
const uint PIN_PWM = 15;
const uint PIN_IRQ = 4;

static volatile bool clockBusy = false;

void clock_callback(uint gpio, uint32_t events);

int main() 
{
    stdio_init_all();
    printf("Init\n");
    
    // LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // LOG
    gpio_init(PIN_LOG);
    gpio_set_dir(PIN_LOG, GPIO_OUT);

    // DAC
    gpio_init(PIN_1);
    gpio_init(PIN_2);
    gpio_init(PIN_3);
    gpio_init(PIN_4);
    gpio_set_dir(PIN_1, GPIO_OUT);
    gpio_set_dir(PIN_2, GPIO_OUT);
    gpio_set_dir(PIN_3, GPIO_OUT);
    gpio_set_dir(PIN_4, GPIO_OUT);

    // PWM
    gpio_set_function(PIN_PWM, GPIO_FUNC_PWM);
    uint sliceNum = pwm_gpio_to_slice_num(PIN_PWM);
    uint chanNum = pwm_gpio_to_channel(PIN_PWM);
     pwm_set_wrap(sliceNum, 2500); // 50 kHz
    pwm_set_chan_level(sliceNum, chanNum, 1250); // duty 50%
    pwm_set_enabled(sliceNum, true);

    // IRQ
    gpio_set_irq_enabled_with_callback(PIN_IRQ, GPIO_IRQ_EDGE_RISE, true, &clock_callback);

    printf("Start\n");
    gpio_put(LED_PIN, 1);

    while (true) 
    {
        // TODO other tasks
    }

    printf("Done\n");
    gpio_put(LED_PIN, 0);
    return 0;
}

void clock_callback(uint gpio, uint32_t events) 
{
    if(clockBusy)
    {
        printf("!!! overlap !!!\n");
        gpio_put(LED_PIN, 0);
        return;
    }
    clockBusy = true;
    gpio_put(PIN_LOG, true);

    //  ~ 440 Hz
    static uint step = 0;
    static uint stepLevel = 0;
    static const uint stepCount = 50000 / 440; // 50000 Hz / 440 Hz ~ 113 steps 
    static const uint stepMod = stepCount / MAX_DAC_LEVEL; // ~ 7 steps per level bit

    if(step >= stepCount)
    {
        step = 0;
        stepLevel = 0;
    }
    
    gpio_put(PIN_1, stepLevel & 0x01);
    gpio_put(PIN_2, stepLevel & 0x02);
    gpio_put(PIN_3, stepLevel & 0x04);
    gpio_put(PIN_4, stepLevel & 0x08);
    
    step++;

    if(step % stepMod == 0)
    {
        stepLevel++;
    }

    gpio_put(PIN_LOG, false);
    clockBusy = false;
}
