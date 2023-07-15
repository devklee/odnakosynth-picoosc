#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

const uint SAMPL_FREQ = 440;
const uint CLOCK_US = 50; // ~ 50 us, 20 kHz
const uint LED_PIN = PICO_DEFAULT_LED_PIN;
const uint MAX_PWM_LEVEL = 2500; // f ~ 50 kHz
const uint PIN_SQUARE = 13; // Square
const uint PIN_SAW = 15; // Saw

// Use alarm 0
#define ALARM_NUM 0
#define ALARM_IRQ TIMER_IRQ_0

// Alarm interrupt handler
static volatile bool clockBusy = false;

void clock_irq(void);
void clock_in_us(uint32_t delay_us);
uint32_t pwm_set_freq_duty(uint slice_num, uint chan, uint32_t f, int d);

int main() 
{
    stdio_init_all();
    printf("Init\n");
    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // Saw
    gpio_set_function(PIN_SAW, GPIO_FUNC_PWM);
    uint sliceNum = pwm_gpio_to_slice_num(PIN_SAW);
    pwm_set_wrap(sliceNum, MAX_PWM_LEVEL);
    pwm_set_gpio_level(PIN_SAW, 0);

    // Square, set PWM to ~440 Hz, 50% duty
    gpio_set_function(PIN_SQUARE, GPIO_FUNC_PWM);
    sliceNum = pwm_gpio_to_slice_num(PIN_SQUARE);
    uint chanNum = pwm_gpio_to_channel(PIN_SQUARE);
    pwm_set_freq_duty(sliceNum, chanNum, SAMPL_FREQ, 50);

    printf("Start\n");
    gpio_put(LED_PIN, 1);

    // Enable pwm gpio 13 and 15 simultaneous
    pwm_set_mask_enabled(0xC0);
    
    // Init clock ~ 50us, 50 kHz
    clock_in_us(CLOCK_US);

    while (true) 
    {
        // TODO other tasks
    }

    printf("Done\n");
    gpio_put(LED_PIN, 0);
    return 0;
}

void clock_irq(void) 
{    
    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
    // Set next clock    
    clock_in_us(CLOCK_US);
    if(clockBusy)
    {
        printf("!!! overlap !!!\n");
        gpio_put(LED_PIN, 0);
        return;
    }
    clockBusy = true;

    // saw ~ 440 Hz
    static uint step = 0;
    static const uint stepCount = 20000 / SAMPL_FREQ; // 20000 Hz / 440 Hz ~ 45 steps 
    static const uint pwmLevelStep = MAX_PWM_LEVEL / stepCount; // ~ 55

    if(step >= stepCount)
    {
        step = 0;
    }
    pwm_set_gpio_level(PIN_SAW, step*stepCount);
    step++;
    
    clockBusy = false;
}

void clock_in_us(uint32_t delay_us) 
{
    // Enable the interrupt for our alarm (the timer outputs 4 alarm irqs)
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
    // Set irq handler for alarm irq
    irq_set_exclusive_handler(ALARM_IRQ, clock_irq);
    // Enable the alarm irq
    irq_set_enabled(ALARM_IRQ, true);
    // Enable interrupt in block and at processor

    // Alarm is only 32 bits so if trying to delay more
    // than that need to be careful and keep track of the upper
    // bits
    uint64_t target = timer_hw->timerawl + delay_us;

    // Write the lower 32 bits of the target time to the alarm which
    // will arm it
    timer_hw->alarm[ALARM_NUM] = (uint32_t) target;
}

uint32_t pwm_set_freq_duty(uint slice_num, uint chan, uint32_t f, int d)
{
    uint32_t clock = 125000000;
    uint32_t divider16 = clock / f / 4096 + (clock % (f * 4096) != 0);
    if (divider16 / 16 == 0)
    {
        divider16 = 16;
    }
 
    uint32_t wrap = clock * 16 / divider16 / f - 1;
    pwm_set_clkdiv_int_frac(slice_num, divider16/16, divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, chan, wrap * d / 100);
    return wrap;
}
