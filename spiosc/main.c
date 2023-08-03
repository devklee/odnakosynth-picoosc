#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

static inline uint64_t clock_us(void)
{
	return to_us_since_boot(get_absolute_time());
}

const uint cs_pin = 10;
const uint ldac_pin = 11;
const uint cs_pin2 = 12;


static inline void cs_select(bool select) 
{
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, !select);  
    asm volatile("nop \n nop \n nop");
}

static inline void cs_select2(bool select) 
{
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin2, !select); 
    asm volatile("nop \n nop \n nop");
}

static inline void ldac_select() 
{
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    gpio_put(ldac_pin, 0);  
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    asm volatile("nop \n nop \n nop");
    gpio_put(ldac_pin, 1);
}

int main()
{
    set_sys_clock_khz(125000, true);
    stdio_init_all();
    printf("\n\n---- start ----\n");
    
    // 20 MHz
    spi_init(spi0, 20000000);
    // transfer 16 bit
    spi_set_format(spi0, 16, 0, 0, 0);
    
    gpio_set_function(6, GPIO_FUNC_SPI);       // SCK    PICO_DEFAULT_SPI_SCK_PIN 6 (9)
    gpio_set_function(7, GPIO_FUNC_SPI);       // TX     PICO_DEFAULT_SPI_TX_PIN  7 (10) 

    gpio_init(cs_pin); 
    gpio_set_dir(cs_pin, GPIO_OUT);
    gpio_put(cs_pin, 1);

    gpio_init(cs_pin2); 
    gpio_set_dir(cs_pin2, GPIO_OUT);
    gpio_put(cs_pin2, 1);

    gpio_init(ldac_pin); 
    gpio_set_dir(ldac_pin, GPIO_OUT);
    gpio_put(ldac_pin, 1);

    // debug
    gpio_init(15); 
    gpio_set_dir(15, GPIO_OUT);
    

    printf("---- init done ----\n");

    const uint16_t DAC_config_chan_A = 0b0011000000000000;
    const uint16_t DAC_config_chan_B = 0b1011000000000000;

    volatile uint64_t start = clock_us();  
    volatile uint64_t end = 0;
    bool on = false;
    uint16_t buf[4];
    buf[0] = DAC_config_chan_A;
    buf[1] = DAC_config_chan_B;
    
    buf[2] = DAC_config_chan_A | 0x0fff;
    buf[3] = DAC_config_chan_B | 0x0fff;

    while (true) 
    {    
        uint64_t end = clock_us();        
        if(end - start >= 10)
        {            
            // debug
            gpio_put(15, 1);
            
            on = !on;
            uint16_t *b = on ? buf+2 : buf;
            
            // MCP 4822 num 1
            cs_select(true); 
            spi_write16_blocking(spi0, b, 1);       // chanel A
            cs_select(false);
            cs_select(true); 
            spi_write16_blocking(spi0, b+1, 1);     // chanel B
            cs_select(false);

            // MCP 4822 num 2
            cs_select2(true); 
            spi_write16_blocking(spi0, b, 1);       // chanel A
            cs_select2(false);
            cs_select2(true); 
            spi_write16_blocking(spi0, b+1, 1);     // chanel B
            cs_select2(false);

            ldac_select();                          // sync
            
            start = clock_us(); 
            
            // debug
            gpio_put(15, 0);
        }
    }
    return 0;
}


