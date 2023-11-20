#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"

static uint8_t table[256];
static bool init = false;

// x8 + x7 + x6 + x4 + x2 + 1
const uint8_t poly = 0xd5;


void crc8xInit(void)
{
    for (int i = 0; i < 256; ++i)
    {
        int temp = i;
        for (int j = 0; j < 8; ++j)
        {
            if ((temp & 0x80) != 0)
            {
                temp = (temp << 1) ^ poly;
            }
            else
            {
                temp <<= 1;
            }
        }
        table[i] = (uint8_t)temp;
    }

}

uint8_t crc8xCalculateFast(uint8_t* data, size_t len) 
{
    if(!init)
    {
        crc8xInit();
        init = true;
    }
    
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) 
    {
         crc = table[data[i] ^ crc];
    }
    return crc;
}
