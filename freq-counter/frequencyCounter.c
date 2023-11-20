#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include <math.h>
#include <inttypes.h>
#include "pico/critical_section.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "frequencyCounter.h"
#include "counter.pio.h"
#include "pps.pio.h"

const uint clockPin  = 17;                  // clock 500 kHz
const uint ppsPin    = 18;                  // audio signal f(s), pulse per signal

const uint64_t clockBasis = 500000;         // 500 kHz
const uint64_t ppsBasis = 16;               //  periods
volatile uint32_t prevCount = 0;
volatile bool ppsFlag = false;
uint32_t clockCount = 0;                    // 0xFFFFFFFF--
int dmaChan = 0;
int dmaChan2 = 0;
int dmaChan3 = 0;
PIO pio = pio0;

uint32_t frequency = 0; // xxxxx[.]nnn
const uint64_t nnBasis = 1000;
critical_section_t csFrequency;

const uint64_t ppsX = clockBasis * ppsBasis * nnBasis;

void ppsCallback() 
{
    volatile uint64_t delta = 0;
    volatile uint32_t prevCountNow = prevCount;
    volatile uint32_t clockCountNow = clockCount;
    pio_interrupt_clear(pio, 0);
    if(prevCountNow > clockCountNow)
    {
        delta = prevCountNow - clockCountNow;
    }
    else
    {
        // clock has reached zero and started all over 0xFFFFFFFF again
        delta = 0xFFFFFFFF - clockCountNow + prevCountNow;     
    }
    if(delta > 0)
    {
        uint64_t freq = ppsX / delta + (ppsX % delta != 0);
        critical_section_enter_blocking(&csFrequency);
        frequency = (uint32_t)freq;
        prevCount = clockCountNow;
        critical_section_exit(&csFrequency);
        //printf("delta %u, freq %u\n", (uint32_t)delta, frequency);
        ppsFlag = true;
    }
}

void frequencyCounterInit(void)
{
    critical_section_init(&csFrequency);

    //load programs
    uint offsetClock = pio_add_program(pio, &counter_program);
    uint offsetPps = pio_add_program(pio, &pps_program);

    //PPS program
    //uint smPps = pio_claim_unused_sm(pio, true);
    uint smPps = 0;
    pps_program_init(pio, smPps, offsetPps, ppsPin);

    //clock counter program
    //uint smClock = pio_claim_unused_sm(pio, true);
    uint smClock = 1;
    counter_program_init(pio, smClock, offsetClock, clockPin);

    //setup DMA between SMs
    dmaChan = dma_claim_unused_channel(true);
    dmaChan2 = dma_claim_unused_channel(true);

    // channel 1, this starts and than hands over to the second channel when it is done
    // channel 2 then hands back to channel 1, so we get a continous DMA stream to a single target variable
    dma_channel_config dc = dma_channel_get_default_config(dmaChan);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, false);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_chain_to(&dc, dmaChan2);
    channel_config_set_dreq(&dc, pio_get_dreq(pio, smPps, true));
    dma_channel_configure(dmaChan, &dc,
        &pio->txf[smPps],
        &pio->rxf[smClock],
        0xFFFFFFFF,
        true
    );

    // channel 2 as above
    dma_channel_config dc2 = dma_channel_get_default_config(dmaChan2);
    channel_config_set_transfer_data_size(&dc2, DMA_SIZE_32);
    channel_config_set_read_increment(&dc2, false);
    channel_config_set_write_increment(&dc2, false);
    channel_config_set_dreq(&dc2, pio_get_dreq(pio, smPps, true));
    channel_config_set_chain_to(&dc2, dmaChan);
    dma_channel_configure(dmaChan2, &dc2,
        &pio->txf[smPps],
        &pio->rxf[smClock],
        0xFFFFFFFF,
        false
    );

    // setup DMA from PPS SM to CPU
    dmaChan3 = dma_claim_unused_channel(true);

    dma_channel_config dc3 = dma_channel_get_default_config(dmaChan3);
    channel_config_set_transfer_data_size(&dc3, DMA_SIZE_32);
    channel_config_set_read_increment(&dc3, false);
    channel_config_set_write_increment(&dc3, false);
    channel_config_set_dreq(&dc3, pio_get_dreq(pio, smPps, false));
    dma_channel_configure(dmaChan3, &dc3,
        &clockCount,
        &pio->rxf[smPps],
        0xFFFFFFFF,
        true
    );

    // setup PPS IRQ/ISR
    irq_set_exclusive_handler(PIO0_IRQ_0, ppsCallback);
    irq_set_enabled(PIO0_IRQ_0, true);

    // enable state machines
    pio_sm_set_enabled(pio, smPps, true);
    pio_sm_set_enabled(pio, smClock, true);
}

bool frequencyCounterGetPpsFlag(void)
{
    return ppsFlag;
}

void frequencyCounterResetPpsFlag(void)
{
    ppsFlag = false;
}

uint32_t frequencyCounterGetFrequency(void)
{
     uint32_t freq = 0;
     critical_section_enter_blocking(&csFrequency);
     freq = frequency;
     critical_section_exit(&csFrequency);
     return freq;
}