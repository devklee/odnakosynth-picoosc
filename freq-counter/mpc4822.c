#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "mpc4822.h"
#include "core1.h"

/*
    VDD 1|-\/---|8 CHANNEL A Vout
    !CS 2|  MPC |7 VSS
    SCK 3| 4822 |6 CHANNEL B Vout
SDI(TX) 4|------|5 !LDAC

VDD: +5V, VSS 0V

<-- 16 bit --------------------------------------->
|CH|--|GA|ON|D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0|

CHanell:
0 channel A
1 channel B

GAin:
0 2x gain, 2xVref, 0~4V
1 1x gain, 1xVref, 0~2V

ON:
0 shutdown
1 active

*/


// Pin assignment is important, predefined by Picoboard
const uint sckPin   =  6;       // GP6  ( 9) SCK    PICO_DEFAULT_SPI_SCK_PIN
const uint txPin    =  7;       // GP7  (10) TX     PICO_DEFAULT_SPI_TX_PIN
const uint rxPin    =  8;       // GP8  (11) RX     PICO_DEFAULT_SPI_RX_PIN

// Normal pin out
const uint ldacPin  = 10;       // GP10 (14) LDAC
const uint csPin    = 11;       // GP11 (15) CS

void dacInit(void)
{
    spi_init(spi0, 20000000);
    spi_set_format(spi0, 16, 0, 0, 0);

    gpio_set_function(sckPin, GPIO_FUNC_SPI);
    gpio_set_function(txPin, GPIO_FUNC_SPI);   

    gpio_init(ldacPin); 
    gpio_set_dir(ldacPin, GPIO_OUT);
    gpio_put(ldacPin, true);

    gpio_init(csPin); 
    gpio_set_dir(csPin, GPIO_OUT);
    gpio_put(csPin, true); 
}

// chip select 
static inline void csSelect(bool select) 
{
    asm volatile("nop \n nop \n nop");
    gpio_put(csPin, !select);  // Active low
    asm volatile("nop \n nop \n nop");
}

// move data to dac output
static inline void ldacSelect() 
{
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    gpio_put(ldacPin, 0);  
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    gpio_put(ldacPin, 1);
}

static inline void dacWrite2(uint16_t chan_A, uint16_t chan_B)
{
    csSelect(true);    
    spi_write16_blocking(spi0, &chan_A, 1);
    csSelect(false);   
    csSelect(true); 
    spi_write16_blocking(spi0, &chan_B, 1);
    csSelect(false);
    ldacSelect(); 
}

static inline void dacWrite1(uint16_t chan)
{
    csSelect(true);    
    spi_write16_blocking(spi0, &chan, 1);
    csSelect(false);   
    ldacSelect();
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
}

static inline void dacWriteN(uint16_t chan_value)
{
    for(int i = 0; i < 3; i++)
    {
        dacWrite1(chan_value);
    }
}


void dacWrite(uint8_t flags, uint value)
{
    uint16_t dacValue = 0b0001000000000000;
    dacValue |= value & 0x0fff;
    
    if(flags & 1)
    {
        dacValue |= 0b1000000000000000;
        if(!isCmdMode())
        {
            printf("chanel B, ");
        }
    }
    else
    {
        if(!isCmdMode())
        {
            printf("chanel A, ");
        }
    }
    if(flags & 2)
    {
        if(!isCmdMode())
        {
            printf("half, ");
        }
        dacValue |= 0b0010000000000000;
    }
    else
    {
        if(!isCmdMode())
        {
            printf("full, ");
        }
    }
    if(!isCmdMode())
    {
        printf("value %u\n", value);
    }
    dacWriteN(dacValue);
}

void dacWriteRaw(uint16_t value)
{
    dacWriteN(value);
}
