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
#include "frequencyCounter.h"
#include "core1.h"
#include "midi.h"
#include "midinote.h"

// test
const uint ledPin  = 16;
const uint adcPin  = 28;      // GP28 (34) ADC2

uint processFrequencyStep = 0;
uint32_t freq = 0;
bool freqReaded = false;

static inline uint32_t clock_ms(void)
{
	return to_ms_since_boot(get_absolute_time());
}

void ledBlinkingProcessTask() 
{
    static uint ledProcessBlinkStart = 0;
    static bool started = false;
    static bool ledOn = false;

    if(processFrequencyStep == 0)
    {
        if(started)
        {
            gpio_put(ledPin, true);
            started = false;
            ledOn = false;
            ledProcessBlinkStart = 0;
        }
    }
    else
    {
        if(started)
        {
            if (clock_ms() - ledProcessBlinkStart < 250) return;
            ledOn = !ledOn;
            gpio_put(ledPin, ledOn);
            ledProcessBlinkStart = clock_ms();
        }
        else
        {
            started = true;
            ledOn = false;
            gpio_put(ledPin, ledOn);
            ledProcessBlinkStart = clock_ms();
        }
    }
}

void FrequencyTask(void);

int main()
{
    uint clk = 125000;//200000;
    bool clkset = set_sys_clock_khz(clk, true);
    stdio_init_all();
    board_init();    
    sleep_ms(500);
    printf("\n\n[start init] ==========%dMHz==========\n", clk/1000);    
    
    core1Init();
    midiInit(true, true, true);    
    frequencyCounterInit();
    dacInit();

    gpio_init(ledPin); 
    gpio_set_dir(ledPin, GPIO_OUT);

    // adc init
    adc_init();
    adc_gpio_init(adcPin);
    
    multicore_launch_core1(core1Entry);

    printf("\n[done init]  ==========================\n");    
    
    // test
    gpio_put(ledPin, true);
    
    uint32_t start = clock_ms();  
    uint32_t freqOld = 0;
    uint32_t startTimeout = 0;
    bool started = false;
    while (true) 
    {                            
        midiProcessTask();
        // WARNING: ...tasks() need a time! Be faster.

        ledBlinkingProcessTask();

        FrequencyTask();

        if(isProcessFrequency())
        {
            if(!started)
            {
                if(processFrequencyStep == 0)
                {
                    processFrequencyStep = 1;
                    started = true;
                }
            }
        }
        else
        {
            if(processFrequencyStep != 0)
            {
                processFrequencyStep = 99;
            }
            started = false;
        }
        
        if(frequencyCounterGetPpsFlag())
        {
            freq = frequencyCounterGetFrequency();
            freqReaded = true;

            if(isSendFrequencyToPc())
            {
                queueCore1Item item = {0};
                item.op = 6;
                item.value = freq;
                core1AddItem(&item);
                startTimeout = 0;
            }
                        
            uint32_t dt =  clock_ms() - start;
            if(dt > 250)
            {
                if(freq != freqOld && isShowFrequency())
                {
                    adc_select_input(2);
                    uint16_t raw = adc_read();
                    
                    printf("freq: %5.3f, adc %u\n", (double)freq/1000., raw);
                }
                
                freqOld = freq;
                start = clock_ms();  
            }
            frequencyCounterResetPpsFlag();
        }
        
        // timeout
        if(isSendFrequencyToPc())
        {
            if(startTimeout == 0)
            {
                startTimeout = clock_ms();
            }
            else
            {
                uint32_t delta =  clock_ms() - startTimeout;
                if(delta > 1000)
                {
                    printf("Timeout, send 0 Hz frequenz (%d)\n", delta);
                    queueCore1Item item = {0};
                    item.op = 6;
                    item.value = 0;
                    core1AddItem(&item);
                    startTimeout = 0;
                }
            }
        }
    }
    printf("\n\n[stop!]  ==========================\n");   
    gpio_put(ledPin, 0);
    return 0;
}

void FrequencyTask(void)
{
    static queueCore1Item item;
    static uint32_t startProcess = 0; 
    static uint freqVoltage = 0;
    static uint prevFrequency = 0;
    static uint freqReadCounter = 0;
    static bool half = false;
    switch(processFrequencyStep)
    {
        case 0:     // wait for a start                    
            break;

        case 1:     // start
            printf("Start build frequency table.\n");
            item.op = 4;    // reset
            core1AddItem(&item);
            midiNoteReset();
            startProcess = clock_ms();
            freqVoltage = 0;
            prevFrequency = 0;
            freqReadCounter = 0;
            half = isProcessHalfVoltage();
            processFrequencyStep = 2;
            break;

        case 2:     // set voltage
            freqReadCounter = 0;
            freqReaded = false;
            item.op = 1;
            item.flags = half ? 2 : 0;
            item.value = freqVoltage; 
            core1AddItem(&item);
            processFrequencyStep = 3;
            break;

        case 3:     // wait for frequency
            if(!freqReaded) break;
            freqReaded = false;
            processFrequencyStep = 4;
            break;

        case 4:     // read frequency
            if(freqReadCounter == 0)
            {
                prevFrequency = freq;
                freqReadCounter++;
                processFrequencyStep = 3;
                break;
            }
            freqReadCounter++;
            if(freqReadCounter > 10)
            {
                printf("Too much frequency read attempts, set frequency for %u to %u.\n", freqVoltage, freq);
                processFrequencyStep = 5;
                break;
            }
            if (freq != 0 && prevFrequency == 0)
            {
                prevFrequency = freq;
                processFrequencyStep = 3;
                break;
            }
            if (freq == 0)
            {
                processFrequencyStep = 5;
                break;
            }
            double percDelta =  (double)abs(freq - prevFrequency) / (double)freq;
            prevFrequency = freq;
            if (percDelta > 0.01)
            {                        
                processFrequencyStep = 3;
                break;
            }
            processFrequencyStep = 5;
            break;
        
        case 5:     // save frequency                                    
            if(half)
            {
                midiNoteSetNoteVoltage(freq, freqVoltage+0x2000);
            }
            else
            {
                midiNoteSetNoteVoltage(freq, freqVoltage);
            }
            freqVoltage++;
            if(freqVoltage > 4095)
            {
                // if(half)
                // {
                //     freqVoltage = 0;
                //     half = false;
                //     printf("Readed [half] frequency complitly.\n");
                //     break;
                // }
                
                printf("Readed frequency complitly.\n");
                processFrequencyStep = 99;
                break;
            }            
            processFrequencyStep = 2;
            break;
        
        case 99:
            uint32_t delta =  clock_ms() - startProcess;
            printf("Stop. Time elapsed %u\n", delta);
            processFrequencyStep = 0;
            break;

        default:
            printf("Invalid task step: %d\n.", processFrequencyStep);
            break;
    }
}



