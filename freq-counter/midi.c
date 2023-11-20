#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bsp/board.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico-dco.pio.h"

#include "tusb.h"
#include "midi.h"
#include "midinote.h"
#include "core1.h"
#include "crc8x.h"

const uint midiChannel = 0;
static uint8_t midi_buf[128];

bool processMidiUsbTask = false;
bool processMidiSerialTask = false;
bool loggingMidiEvent = false;
uint ledBlinkStart = 0;

//TODO change to ring buffer?
const uint midiQueueLength = 256;
queue_t queueMidi;

// Pin assignment is important, predefined by Picoboard
const uint midiTxPin   =  4;       // GP4  ( 6) UART1 TX
const uint midiRxPin   =  5;       // GP5  ( 7) UART1 RX

//test pico dco
const uint resetPin    =  13;      // GP13 (17) reset pin
uint sm_reset = 0;
void set_frequency(uint note);

void midiSendEvent(uint8_t *event, uint eventLenght);

bool processMidiRx(uint8_t d);
void processMidiMessage(uint8_t *buf, uint bufLen);
void processSysEx(uint8_t *buf, uint bufLen);
void logMidiEvent(uint8_t *buf, uint bufLen);
void logMidiNote(uint code, uint8_t *buf);
void logCC(uint8_t *buf);
void logPrintSysEx(uint8_t *buf, int count, bool printLn);
void usbMidiTask();
void serialMidiTask();
void ledBlinkingTask();
void onUartRx();
uint decode8in7(uint8_t *src, uint srcLen, uint8_t *dist);
uint encode8in7(uint8_t *src, uint srcLen, uint8_t *dist);
void decimalToBcd(uint32_t value, uint8_t *buf, uint lenghtBcd);
uint32_t bcdToDecimal(uint8_t *buf, uint lenght);

static inline uint32_t clock_ms(void)
{
	return to_ms_since_boot(get_absolute_time());
}

void midiInit(bool logMidiEvent, bool serialTask, bool usbTask)
{
    loggingMidiEvent = logMidiEvent;
    processMidiSerialTask = serialTask;
    processMidiUsbTask = usbTask;

    if(processMidiUsbTask)
    {
        tusb_init();
    }
    
    if(processMidiSerialTask)
    {
        queue_init(&queueMidi, sizeof(uint8_t), midiQueueLength);

        // init serial midi
        uart_init(uart1, 31250);
        gpio_set_function(midiTxPin, GPIO_FUNC_UART);
        gpio_set_function(midiRxPin, GPIO_FUNC_UART);
        uart_set_format(uart1, 8, 1, UART_PARITY_NONE);
        uart_set_hw_flow(uart1, false, false);
        uart_set_fifo_enabled(uart1, false);
        uart_set_translate_crlf(uart1, false);
        irq_set_exclusive_handler(UART1_IRQ, onUartRx);
        irq_set_enabled(UART1_IRQ, true);
        // Now enable the UART to send interrupts - RX only
        uart_set_irq_enables(uart1, true, false);
    }

    //test dco pico
    uint offset = pio_add_program(pio1, &frequency_program);
    sm_reset = pio_claim_unused_sm(pio1, true);
    init_sm_pin(pio1, sm_reset, offset, resetPin);
    pio_sm_set_enabled(pio1, sm_reset, true);
    set_frequency(69);

}

void midiSendPing(void)
{
    uint8_t event[] = { 0xf0, 0x7d, 0x01, 0x00, 0xf7};
    midiSendEvent(event, sizeof(event));
}

void midiSendEvent(uint8_t *event, uint eventLength)
{
    if(processMidiUsbTask)
    {
        // send max 48 byte!!!
        int count = tud_midi_stream_write(0, event, eventLength);
        printf("Send %d bytes.\n", count);
    }
    if(processMidiSerialTask && uart_is_writable(uart1))
    {
        uart_write_blocking(uart1, event, eventLength);
    }    
}

void midiProcessTask(void)
{
    tud_task();
    usbMidiTask();
    serialMidiTask();
    ledBlinkingTask();
}

bool processMidiRx(uint8_t d)
{
    static uint step = 0;
    static uint pos = 1;
    bool handleMessage = false;
    
    if(d == 0xf8)
    {
        //TODO  procces clock
        return true;
    }
    
    switch(step)
    {
        case 0:
            if(d > 0x7f)
            {
                uint code = d < 0xf0 ? (d & 0xf0) : d;                
                memset(midi_buf, 0, sizeof(midi_buf));
                midi_buf[0] = d;
                switch(code)
                {
                    case 0x80:  
                    case 0x90:
                    case 0xa0:
                    case 0xb0:
                    case 0xe0:
                    case 0xf2:
                        step = 1; // 3 byte command
                        break;

                    case 0xc0:  
                    case 0xd0:
                    case 0xf1:
                    case 0xf3:
                    case 0xf5:
                        step = 3; // 2 byte command
                        break;

                    case 0xf6:  
                    case 0xfa:
                    case 0xfb:
                    case 0xfc:
                    case 0xfe:
                    case 0xff:
                        step = 0; // 1 byte command
                        handleMessage = true;
                        break;

                    case 0xf0:
                        step = 4;   // start sysex message
                        pos = 1;
                        break;

                    default:
                        printf("Unknown midi message %02x\n", d);
                }
            }
            break;
        
        case 1: // 3 byte command: first param byte
            if(d > 0x7f)
            {
                printf("Invalid midi data byte %02x, step %d\n", d, step);
                step = 0;
            }
            else
            {
                midi_buf[1] = d;
                step = 2;
            }
            break;

        case 2: // 3 byte command: second param byte
            if(d > 0x7f)
            {
                printf("Invalid midi data byte %02x, step %d\n", d, step);
                step = 0;
            }
            else
            {
                midi_buf[2] = d;
                handleMessage = true;
            }
            break;

        case 3: // 2 byte command: param byte
            if(d > 0x7f)
            {
                printf("Invalid midi data byte %02x, step %d\n", d, step);
                step = 0;
            }
            else
            {
                midi_buf[1] = d;
                handleMessage = true;
            }
            break;

        case 4: // sysex
            if(pos >= sizeof(midi_buf))
            {
                printf("SysEx message too big!\n");
                pos = 1;
                step = 0;
            }
            else if(d == 0xf7)
            {
                midi_buf[pos] = d;
                handleMessage = true;
            }
            else if(d > 0x7f)
            {
                printf("Invalid midi data byte %02x, step %d\n", d, step);
                pos = 1;
                step = 0;
            }
            else
            {
                midi_buf[pos] = d;
                pos++;
            }
            break;

        default:
            printf("Invalid midi rx step!\n");
            step = 0;
            pos = 1;
            handleMessage = false;
            break;
    }
    
    if(handleMessage)
    {
        processMidiMessage(midi_buf, sizeof(midi_buf));
        step = 0;
        pos = 1;
        return true;
    }

    return false;
}

void processMidiMessage(uint8_t *buf, uint bufLen)
{
    if(logMidiEvent && !isCmdMode())
    {
        logMidiEvent(buf, bufLen);
    }
    
    uint cn = buf[0] & 0x0f;    
    uint code = (buf[0] & 0xf0) >> 4;
    
    if(code == 0x09 && cn == midiChannel)
    {
        //note on
        midiNoteSendMidiNote(buf[1]);

        //test pico dco, reset pin
        set_frequency(buf[1]);
    }
    else if(code == 0x08 && cn == midiChannel)
    {
        //TODO note off
    }
    else if(code == 0x0b && cn == midiChannel)
    {
        // CC
        if(buf[1] == 120 || buf[1] == 121 || buf[1] == 123)
        {
            queueCore1Item item = {0};
            item.op = 4;    // reset
            core1AddItem(&item);            
        }
        else if(buf[1] == 113 || buf[1] == 114)
        {
            int amount = 1;
            // MiniLab3 Relative1 62 <==<== 63 <== 0 ==> 65 ==>==> 66
            if(buf[1] == 113)
            {
                amount = (buf[2] == 62 || buf[2] == 66) ? 100 : 10;
            }
            else
            {
                amount = (buf[2] == 62 || buf[2] == 66) ? 10 : 1;
            }
            if(buf[2] == 62 || buf[2] == 63)
            {
                amount *= -1;
            }
            midiNoteSendTuneNote(amount);
        }
        else if(buf[1] == 115)
        {
            if(buf[2] > 0)
            {
                midiNoteSaveMidiNote();
            }
        }
        else if(buf[1] == 112)
        {
            // MiniLab3 Relative2 125 <== 0 ==> 2
            midiNoteSetHalf(buf[2] >= 64);
        }        
    }
    else if(buf[0] == 0xf0)
    {
       processSysEx(buf, bufLen);
    }
}

//test pico dco, reset pin
void set_frequency(uint note) 
{
    if(note > 127)
    {
        printf("Wrong note number %u\n", note);
        return;
        
    }        
    uint32_t clk_div = 142045;
    float pp = (float)note - 69.0f;
    pp = pp / 12.0f;
    float freq = pow(2, pp) * 440.0f;
    if(freq != 0.0f)
    {
        clk_div = clock_get_hz(clk_sys) / 2 / freq;
    }
    printf("set note %u frequency %f, pp %f, clk_div %u, clk_sys %u\n", note, freq, pp, clk_div, clock_get_hz(clk_sys));
    pio_sm_put(pio1, sm_reset, clk_div);
    pio_sm_exec(pio1, sm_reset, pio_encode_pull(false, false));
    pio_sm_exec(pio1, sm_reset, pio_encode_out(pio_y, 32));
}


void processSysEx(uint8_t *buf, uint bufLen)
{    
    /* 0  1    2    3    NN   NN+1  
       f0 7d | 01 | 00 | XX | f7        
               device number
                    opCode
                         data 0...N 8-in-7 code
    */

    uint dataLen = 0;
    bool dataOK = false;

    if(bufLen >= 4)
    {
        for(int i = 0; i < bufLen; i++)
        {
            if(i == 0)
            {
                if(buf[i] != 0xf0) break;
            } 
            else if(i == 1)
            {
                if(buf[i] != 0x7d) break;
            } 
            else if(i == 2)
            {
                if(buf[i] != 0x01) break;
            } 
            else
            {
                if(buf[i] == 0xf7)
                {
                    dataOK = true;
                    break;
                } 
                dataLen++;
            }
        }
    }

    if(!dataOK || dataLen == 0)
    {
        printf("Missing SysEx format!\n");
        return;
    }

    queueCore1Item item = {0};
    uint8_t *data = buf+4;
    dataLen--;
    switch(buf[3])
    {
        case 00:        // ping
            item.op = 0;
            core1AddItem(&item);
            break;

        case 01:        // set voltage
            /*
                01 ee bb bb bb (8in7)
                   flags
                      voltage in BCD ==> 0..4096
                01 ff vv vv
            */
            if(dataLen != 4) break;
            uint8_t dist[5];
            if(decode8in7(data, 4, dist) != 3) break;
            item.op = 1;
            item.flags = dist[0];
            item.value = bcdToDecimal(dist+1, 2); 
            core1AddItem(&item);
            break;

        case 02:        // frequency
            /*
                02 ee bb bb (8in7)
                   counter in BCD ==> 0..9999
                02 cc cc
            */
            if(dataLen != 3) break;
            uint8_t dist2[5];
            if(decode8in7(data, 3, dist2) != 2) break;
            item.op = 5;
            item.value = bcdToDecimal(dist2, 2);
            core1AddItem(&item);
            break;

        case 03:        // send midi octave table
            if(dataLen < 1) break;
            item.op = 3;
            item.value = *data;
            core1AddItem(&item);            
            break;
        
        case 04:        // recive midi octave table 
            /*
                f0 7d 01 03 ee oo nn nn nn nn ... dd f7 (8in7)
            */
            if(dataLen != 30) break;
            uint8_t valBcd[26];
            if(decode8in7(data, 30, valBcd) != 26) break;
            uint8_t crc = crc8xCalculateFast(valBcd, 25);
            uint8_t crcMessage = valBcd[25];
            if(crc != crcMessage)
            {
                printf("Wrong crc sum %d != %d\n", crc, crcMessage);
                break;
            }
            uint8_t num = bcdToDecimal(valBcd, 1);
            uint16_t odata[8] = {0};
            for(int i = 0; i < 8; i++)
            {
                odata[i] = bcdToDecimal(valBcd+1+i*3, 3);
            }
            midiNoteSetData(num, odata);
            printf("Recived octave data %d\n", num);
            break;
        
        default:
            printf("Unknown SysEx command: %u\n", buf[3]);
            break;
    }
}

void logMidiEvent(uint8_t *buf, uint bufLen)
{    
    uint cn = buf[0] & 0x0f;    
    uint code = (buf[0] & 0xf0) >> 4;
    if(code == 0x08 || code == 0x09)
    {
        // note off or on
        printf("%-2u         ", cn);
        logMidiNote(code, buf);
    }
    else if(code == 0x0a)
    {
        printf("%-2u  AFTT   ", cn);
        logMidiNote(code, buf);
    }
    else if(code == 0x0b)
    {
        printf("%-2u  CC     %-3u  %-3u  ", cn, buf[1], buf[2]);
        logCC(buf);
    }
    else if(code == 0x0c)
    {
        printf("%-2u  PCNG   %-3u       select sound %u", cn, buf[1], buf[2]+1);
    }
    else if(code == 0x0d)
    {
        printf("%-2u  PRESS  %-3u  AFTT", cn, buf[1]);
    }
    else if(code == 0x0e)
    {
        printf("%-2u  PBND   %-3u  %-3u  Pitch Bend", cn, buf[1], buf[2]);
    }
    else if(buf[0] == 0xf2)
    {
        printf("    Song Position");
    }
    else if(buf[0] == 0xf3)
    {
        printf("    Song Select");
    }
    else if(buf[0] == 0xf5)
    {
        printf("    Bus Select");
    }
    else if(buf[0] == 0xf6)
    {
        printf("    Tune Select");
    }
    else if(buf[0] == 0xf8)
    {
        // too many clock's messges
        //printf("    CLOCK");
        return;
    }
    else if(buf[0] == 0xfa)
    {
        printf("    Start song");
    }
    else if(buf[0] == 0xfb)
    {
        printf("    Continue Song");
    }
    else if(buf[0] == 0xfc)
    {
        printf("    Stop Song");
    }
    else if(buf[0] == 0xfe)
    {
        printf("    Active Sencing");
    }
    else if(buf[0] == 0xff)
    {
        printf("    RESET");
    }
    else if(buf[0] == 0xf0)
    {
        uint count = 0;
        for(int i = 0; i < bufLen; i++)
        {
            if(buf[i] == 0xf7)
            {
                count = i + 1;
                break;
            }
        }
        logPrintSysEx(buf, count, false);
    }
    else
    {
        printf("%-02x       %-02x   %-02x", buf[0], buf[1], buf[2]);
    }
    printf("\n");
}

void logPrintSysEx(uint8_t *buf, int count, bool printLn)
{
    printf("    SysEx  %-3u  ", count);
    for(int i = 0; i < count; i++)
    {
        printf("%02x ", buf[i]);
    }
    if(printLn)
    {
        printf("\n");
    }
}

void logCC(uint8_t *buf)
{
    switch(buf[1])
    {
        case   0: printf("select bank %u", buf[2]+1); break;
        case   1: printf("modulation wheel"); break;
        case   2: printf("breath controller"); break;
        case   4: printf("foot peda"); break;
        case   5: printf("portamento time"); break;
        case   7: printf("volume level"); break;
        case  10: printf("pan"); break;
        case  11: printf("expression"); break;
        case  32: printf("select subbank %u", buf[2]+1); break;
        case  64: printf("sustain %s", buf[2] >= 64 ? "on" : "off"); break;
        case  65: printf("portamento %s", buf[2] >= 64 ? "on" : "off"); break;
        case  66: printf("sostenuto (hold) %s", buf[2] >= 64 ? "on" : "off"); break;
        case  67: printf("soft pedal %s", buf[2] >= 64 ? "on" : "off"); break;
        case  68: printf("legato %s", buf[2] >= 64 ? "on" : "off"); break;
        case  69: printf("hold notes %s", buf[2] >= 64 ? "on" : "off"); break;
        case 120: printf("all sound off, mutes all sound"); break;
        case 121: printf("reset all controllers"); break;
        case 123: printf("all notes off, mutes all sounding notes"); break;
    }
}

void logMidiNote(uint code, uint8_t *buf)
{
    int oct = 0;
    uint note = buf[1];
    if(note <= 11)                      { oct =-2;              }
    else if(note >= 12 && note <= 23)   { oct =-1; note -= 12;  }
    else if(note >= 24 && note <= 35)   { oct = 0; note -= 24;  }
    else if(note >= 36 && note <= 47)   { oct = 1; note -= 36;  }
    else if(note >= 48 && note <= 59)   { oct = 2; note -= 48;  }
    else if(note >= 60 && note <= 71)   { oct = 3; note -= 60;  }
    else if(note >= 72 && note <= 83)   { oct = 4; note -= 72;  }
    else if(note >= 84 && note <= 95)   { oct = 5; note -= 84;  }
    else if(note >= 96 && note <= 107)  { oct = 6; note -= 96;  }
    else if(note >= 108 && note <= 119) { oct = 7; note -= 108; }
    else if(note >= 120)                { oct = 8; note -= 120; }
    
    switch(note)
    {
        case  0: printf("C%-3d",  oct); break;
        case  1: printf("C%-2d#", oct); break;
        case  2: printf("D%-3d", oct); break;
        case  3: printf("D%-2d#", oct); break;
        case  4: printf("E%-3d",  oct); break;
        case  5: printf("F%-3d",  oct); break;
        case  6: printf("F%-2d#", oct); break;
        case  7: printf("G%-3d",  oct); break;
        case  8: printf("G%-2d#", oct); break;
        case  9: printf("A%-3d",  oct); break;
        case 10: printf("A%-2d#", oct); break;
        case 11: printf("B%-3d",  oct); break;

        default: printf("..."); break;
    }

    if(code == 0x08)
    {
        printf(" 0");
    }
    else if(code == 0x0a)
    {
        printf(" %-3u", buf[2]);
    }
    else
    {
        char *velocity = ">>>>> ffff";
        if(buf[2] < 113)     { velocity = ">>>>   fff";  }
        else if(buf[2] < 97) { velocity = ">>>    ff";   }
        else if(buf[2] < 81) { velocity = ">>     f";    }
        else if(buf[2] < 65) { velocity = ">      mf";   }
        else if(buf[2] < 54) { velocity = "<      mp";   }
        else if(buf[2] < 43) { velocity = "<<     p";    }
        else if(buf[2] < 32) { velocity = "<<<    pp";   }
        else if(buf[2] < 21) { velocity = "<<<<   ppp";  }
        else if(buf[2] < 9)  { velocity = "<<<<<  pppp"; }
        printf(" %-3u  %s", buf[2], velocity);
    }
}

void usbMidiTask() 
{
    if (!processMidiUsbTask || !tud_midi_available())
    {
        return;
    }

    ledBlinkStart = clock_ms();
    board_led_write(true);
    uint8_t packet[4];
    tud_midi_packet_read(packet);
    for(int i = 1; i < 4; i++)
    {
        if(processMidiRx(packet[i]))
        {
            break;
        }
    }
}

void ledBlinkingTask() 
{
    if (clock_ms() - ledBlinkStart < 50) return;
    board_led_write(false);
}

void serialMidiTask() 
{
    if(!processMidiSerialTask)
    {
        return;
    }
    
    uint8_t data = 0;
    if (!queue_try_remove(&queueMidi, &data)) 
    {
        return;
    }
    
    ledBlinkStart = board_millis();
    board_led_write(true);
    processMidiRx(data);
}

void onUartRx()
{
     while (uart_is_readable(uart1))
     {
        uint8_t data = uart_getc(uart1);
        queue_try_add(&queueMidi, &data);        
        //test
        //uart_putc(uart1, data);
     }
}

/* 8-in-7 code

   DATA ( 1Set = 8bit x 7Byte )
   b7     ~      b0   b7     ~      b0   b7   ~~    b0   b7     ~      b0
   +-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+  +-+-+-~~-+-+-+  +-+-+-+-+-+-+-+-+
   | | | | | | | | |  | | | | | | | | |  | | |    | | |  | | | | | | | | |
   +-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+  +-+-+-~~-+-+-+  +-+-+-+-+-+-+-+-+
         7n+0               7n+1          7n+2 ~~ 7n+5         7n+6

    MIDI DATA ( 1Set = 7bit x 8Byte )
      b7b7b7b7b7b7b7     b6    ~     b0     b6 ~~    b0     b6    ~     b0
   +-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+  +-+-+-~~-+-+-+  +-+-+-+-+-+-+-+-+
   |0| | | | | | | |  |0| | | | | | | |  |0| |    | | |  |0| | | | | | | |
   +-+-+-+-+-+-+-+-+  +-+-+-+-+-+-+-+-+  +-+-+-~~-+-+-+  +-+-+-+-+-+-+-+-+
   7n+6,5,4,3,2,1,0         7n+0          7n+1 ~~ 7n+5         7n+6

   
    i                 k+i+1
*/

uint decode8in7(uint8_t *src, uint srcLen, uint8_t *dist)
{
    uint l = 0;
    for(uint i = 0; i < srcLen; i += 8)
    {
        uint shift = 1;
        for(uint k = 0; k < 7 && k + i + 1 < srcLen; k++)
        {
            dist[l] = src[k + i + 1] | (src[i] & shift ? 0x80 : 0);
            shift = shift << 1;
            l++;
        }
    }
    return l;
}

uint encode8in7(uint8_t *src, uint srcLen, uint8_t *dist)
{
    uint l = 0;
    for(uint i = 0; i < srcLen; i += 7)
    {
        uint shift = 1;
        uint flags = l;
        dist[flags] = 0;
        l++;
        for(uint k = 0; k < 7 && k + i < srcLen; k++)
        {
            dist[l] = src[k + i] & 0x7f;
            dist[flags] = dist[flags] | (src[k + i] & 0x80 ? shift : 0) ;
            shift = shift << 1;
            l++;
        }
    }
    return l;
}

void decimalToBcd(uint32_t value, uint8_t *buf, uint lenghtBcd)
{
    int l = lenghtBcd - 1;
    while(l >= 0)
    {
        buf[l] = value % 10;        
        value /= 10;
        buf[l] += (value % 10)<<4;
        value /= 10;
        l--;
    }
}

uint32_t bcdToDecimal(uint8_t *buf, uint lenght)
{
    uint32_t value = 0;
    uint base = 1;
    int l = lenght - 1;
    while(l >= 0)
    {
        value += (buf[l] & 0x0f) * base;        
        base *= 10; 
        value += ((buf[l] & 0xf0)>>4) * base;
        base *= 10; 
        l--;
    }

    return value;
}

void midiSendFreq(uint counter, uint32_t value)
{
    //printf("midiSendFreq: %u, %lu\n", value, counter);
    
    // f0 7d 01 02 ee cc cc nn nn nn nn f7
    uint8_t valBcd[6] = {0};
    decimalToBcd(counter, valBcd, 2);
    decimalToBcd(value, valBcd+2, 4);    

    uint8_t val8in7[7] = {0};
    encode8in7(valBcd, 6, val8in7);
    
    uint8_t event[] = { 
        0xf0, 0x7d, 0x01, 0x02, 
        val8in7[0], val8in7[1], val8in7[2],
        val8in7[3], val8in7[4], val8in7[5],
        val8in7[6],
        0xf7
    };
    
    midiSendEvent(event, sizeof(event));
}

void midiSendNoteTable(uint num)
{
    // f0 7d 01 03 ee oo nn nn nn nn ... dd f7
    printf("Send octave data %d\n", num);
    if(num < 1 || num > 16)
    {
        return;
    }

    uint16_t data[8];
    midiNoteGetData(num, data);

    // num + data + crc
    // 1 + 8*3 + 1 = 26
    uint8_t valBcd[26];
    decimalToBcd(num, valBcd, 1);
    for(int i = 0; i < 8; i++)
    {
        decimalToBcd(data[i], valBcd+1+i*3, 3);
    } 
    uint8_t crc = crc8xCalculateFast(valBcd, 25);
    valBcd[25] = crc;

    // 26 / 7 => +4 byte for bits: 26 + 4 = 30   
    uint8_t val8in7[30];
    encode8in7(valBcd, 26, val8in7);
    uint8_t event[35] = { 0xf0, 0x7d, 0x01, 0x04 };
    for(int i = 0; i < 30; i++)
    {
        event[i+4] = val8in7[i];
    }
    event[34] = 0xf7;
    
    //logPrintSysEx(event, 35, true);

    // max 48 byte!!!
    midiSendEvent(event, 35);
}