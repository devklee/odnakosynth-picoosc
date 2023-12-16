#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "pico-dco.pio.h"

#include "midi.h"
#include "midinote.h"
#include "core1.h"
#include "mpc4822.h"


static unsigned int lastMidiNote = 0;
static unsigned int lastVoltage = 0;
static int freqStAdjustingChanA = 0;
static int freqCtAdjustingChanA = 0;
static int freqStAdjustingChanB = 0;
static int freqCtAdjustingChanB = 0;
static int freqStAdjustingSub   = 0;
static int freqCtAdjustingSub   = 0;
static const double freqFactor = 1000.0;
static uint32_t start = 0;

// dac
static int valA = 0;
static bool halfA = false;
static int valB = 0;
static bool halfB = false;
static bool setVoltageFromLastMidiNote = false;

//test pico dco
const uint adcPin       =   28;         // GP28 (34) ADC2

const uint resetPinA    =   13;         // GP13 (17) reset pin A (PWM_B)
const uint waitPin      =   14;         // GP14 (19) WAIT 1 GPIO 14
const uint resetPinB    =   15;         // GP15 (20) reset pin B (PWM_B)
const uint subbassPin   =   17;         // GP17 (22) pin subbass (PWM_B)
const uint syncPin      =   18;         // GP18 (24) wire <----> GP14 (19) for a hardware pio sm sync
const uint gatePin      =   19;         // GP19 (25) gate pin 

const bool syncON       =   false;
const bool syncOFF      =   true;
static uint countY      =   24;
static bool syncFreq    =   false;

static uint sm_resetA   =   0;
static uint sm_resetB   =   0;
static uint sm_subbass  =   0;
static uint32_t resetMask = 0;

unsigned int midiNoteGetVoltageFromMideiNote(uint16_t note);
void midiNoteSetVoltageToMideiNote(uint16_t note, uint16_t voltage);
uint midiNoteGetVoltagefromFreq(double freq);
void midiNoteSetChanFrequency(double freqChanA, double freqChanB, double freqSub);
void midiNoteSetFrequencyFromNote(uint note);

#define MAX_MIDI_NOTE 127
static uint16_t MidiNoteToVoltage[128] = 
{    
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,5,6,7,8,9,10,11,
    13,14,15,17,18,20,22,24,
    26,28,30,32,35,37,40,43,
    46,49,53,56,60,64,69,74,
    80,85,91,99,105,111,120,128,
    135,145,152,162,172,183,195,210,
    221,237,253,268,284,303,318,338,
    360,385,406,433,458,493,518,546,
    579,618,655,691,733,786,829,880,
    931,1005,1061,1116,1189,1268,1337,1421,
    1504,1612,1706,1799,1934,2055,2177,2321,
    2465,2613,2773,2915,3112,4095,4095,4095,
    4095,0,0,0,0,0,0,0
};

static inline uint32_t clock_ms(void)
{
	return to_ms_since_boot(get_absolute_time());
}

void midiNoteInit(void)
{
    // adc_init();
    // adc_gpio_init(adcPin);
    // adc_select_input(2);    // GPIO28

    gpio_init(syncPin); 
    gpio_set_dir(syncPin, GPIO_OUT);
    gpio_put(syncPin, syncOFF);

    gpio_init(waitPin); 
    gpio_set_dir(waitPin, GPIO_IN);
    gpio_put(waitPin, syncOFF);

    gpio_init(gatePin); 
    gpio_set_dir(gatePin, GPIO_OUT);
    gpio_put(gatePin, false);

    uint offset = pio_add_program(pio1, &frequency_program);
    sm_resetA = pio_claim_unused_sm(pio1, true);
    init_sm_pin(pio1, sm_resetA, offset, resetPinA, syncPin);    
    
    sm_resetB = pio_claim_unused_sm(pio1, true);
    init_sm_pin(pio1, sm_resetB, offset, resetPinB, syncPin);
    
    sm_subbass = pio_claim_unused_sm(pio1, true);
    init_sm_pin(pio1, sm_subbass, offset, subbassPin, syncPin);
    
    resetMask = (1u << sm_resetA)|(1u << sm_resetB)|(1u << sm_subbass);
    pio_set_sm_mask_enabled(pio1, resetMask, false);

    printf("mask %u, %u, %u, %u\n", resetMask, sm_resetA, sm_resetB, sm_subbass);

    start = clock_ms();

    midiNoteReset2();
}

void midiNoteCmd(uint cmd)
{
    if(cmd == 1)
    {
        setVoltageFromLastMidiNote = true;
    }
    else if(cmd == 2)
    {
        setVoltageFromLastMidiNote = false;        
    }
    printf("setVoltageFromLastMidiNote: %d\n", setVoltageFromLastMidiNote);
}

void midiNoteTask(void)
{
    uint32_t dt =  clock_ms() - start;
    if(dt > 500)
    {
        // 12-bit conversion, assume max value == ADC_VREF == 3.3 V
        // const float conversion_factor = 3.3f / (1 << 12);
        // uint16_t result = adc_read();
        // cmdPrint("Raw value: 0x%03x, voltage: %f V", result, result * conversion_factor);
        start = clock_ms();
    }
}

void midiNoteSetChanFrequency(double freqChanA, double freqChanB, double freqSub) 
{
    if(freqChanA <= 30.0 || freqChanA > 8400.0)
    {
        printf("Wrong frequency ChanA %f\n", freqChanA);
        freqChanA = 30.0;
    }        

    if(freqChanB <= 30.0 || freqChanB > 8400.0)
    {
        printf("Wrong frequency ChanB %f\n", freqChanB);
        freqChanB = 30.0;
    }        

    if(freqSub <= 30.0 || freqSub > 8400.0)
    {
        printf("Wrong frequency Sub %f\n", freqSub);
        freqSub = 30.0;
    }        


    uint32_t clkDivChanA = clock_get_hz(clk_sys) / 4 / freqChanA;
    uint32_t clkDivChanB = clock_get_hz(clk_sys) / 4 / freqChanB;
    uint32_t clkDivSub = clock_get_hz(clk_sys) / 4 / freqSub;
    if(!isCmdMode())
    {
        printf("set frequency ChanA %f, clk div %u, frequency ChanB %f, clk div %u, frequency sub %f, clk div %u\n", 
            freqChanA, clkDivChanA, freqChanB, clkDivChanB, freqSub, clkDivSub);
    }
    
    //pio_set_sm_mask_enabled(pio1, resetMask, false);
    if(syncFreq)
    {
        gpio_put(syncPin, syncOFF);
    }
        
    pio_sm_put(pio1, sm_resetA, clkDivChanA);
    pio_sm_exec(pio1, sm_resetA, pio_encode_pull(false, false));
    pio_sm_exec(pio1, sm_resetA, pio_encode_out(pio_y, countY));

    pio_sm_put(pio1, sm_resetB, clkDivChanB);
    pio_sm_exec(pio1, sm_resetB, pio_encode_pull(false, false));
    pio_sm_exec(pio1, sm_resetB, pio_encode_out(pio_y, countY));

    pio_sm_put(pio1, sm_subbass, clkDivSub);
    pio_sm_exec(pio1, sm_subbass, pio_encode_pull(false, false));
    pio_sm_exec(pio1, sm_subbass, pio_encode_out(pio_y, countY));

    //pio_enable_sm_mask_in_sync(pio1, resetMask);
    if(syncFreq)
    {
        gpio_put(syncPin, syncON);
    }
}

void midiNoteReset(void)
{
    printf("midi note reset\n");
    gpio_put(gatePin, false);
    syncFreq = false;
}

void midiNoteReset2(void)
{
    valA = 0;
    halfA = false;
    valB = 0;
    halfB = false;

    printf("midi note reset2\n");
    
    gpio_put(gatePin, false);

    dacWrite(0, 0); // A 0, full
    dacWrite(1, 0); // B 0, full
    
    freqStAdjustingChanA = 0;
    freqCtAdjustingChanA = 0;
    freqStAdjustingChanB = 0;
    freqCtAdjustingChanB = 0;
    freqStAdjustingSub = 0;
    freqCtAdjustingSub = 0;

    syncFreq = false;

    // init osc
    gpio_put(syncPin, syncOFF);
    sleep_ms(20);
    pio_set_sm_mask_enabled(pio1, resetMask, false);    
    midiNoteNoteOn(69, 0);
    pio_enable_sm_mask_in_sync(pio1, resetMask);
    gpio_put(syncPin, syncON);
}

unsigned int midiNoteGetVoltageFromMideiNote(uint16_t note)
{
    if(note > MAX_MIDI_NOTE)
    {
        return 0;
    }
    return MidiNoteToVoltage[note];
}

void midiNoteSetVoltageToMideiNote(uint16_t note, uint16_t voltage)
{
    if(note > MAX_MIDI_NOTE)
    {
        return;
    }
    MidiNoteToVoltage[note] = voltage;
    lastVoltage = voltage;
}

uint midiNoteGetVoltagefromFreq(double freq)
{
    return (uint)ceil(0.000004*freq*freq + 0.446406*freq - 12.3573);
}

void midiNoteControl(uint note, uint velocity)
{
    // C-2 reset
    if(note == 0)
    {
        queueCore1Item item = {0};
        item.op = 4;    // reset
        core1AddItem(&item);    
    }
    else if(note == 1)
    {
        //C#-2
        midiNoteReset2();
    }
    else if(note == 2)
    {
        //D-2
        gpio_put(syncPin, syncOFF);
    }
    else if(note == 3)
    {
        //D#-2
        syncFreq = !syncFreq;
        printf("sync freq: %d\n", syncFreq);
    }
    else if(note == 4)
    {
        //E-2
        gpio_put(syncPin, syncON);
    }
    else if(note == 5)
    {
        //F-2
    }
    else if(note == 6)
    {
        //F#-2
    }
}

// core1
void midiNoteNoteOn(uint note, uint velocity)
{
    // C0 -- C7
    if(note < 24)
    {        
        note = 23;        
    } 
    else if(note > 119)
    {
        note = 120;
    }
    lastMidiNote = note;

    midiNoteSetFrequencyFromNote(lastMidiNote);

    gpio_put(gatePin, true);
}

void midiNoteSetFrequencyFromNote(uint note)
{
    double pp = (double)note - 69.0;
    pp = pp / 12.0;
    double freq = pow(2, pp) * 440.0;
    
    //100: 0.000250858
    //1000: 2,50858329719984e-5
    static const double factor = 2.50858329719984E-5;
    double freqA = freq * pow(10, (freqStAdjustingChanA*freqFactor + freqCtAdjustingChanA) * factor);
    double freqB = freq * pow(10, (freqStAdjustingChanB*freqFactor + freqCtAdjustingChanB) * factor);
    double freqS = freq * pow(10, (freqStAdjustingSub*freqFactor + freqCtAdjustingSub) * factor);

    uint voltageA = midiNoteGetVoltagefromFreq(freqA);
    uint voltageB = midiNoteGetVoltagefromFreq(freqB);

    // for a get voltage table
    if(setVoltageFromLastMidiNote )
    {
        voltageA = lastVoltage;
    }
    else
    {
        if(isUseVoltageTable())
        {
            voltageA = midiNoteGetVoltageFromMideiNote(note);
        }
    }
    
    if(voltageA > 4095)
    {
        voltageA = 4095;
    }

    if(voltageB > 4095)
    {
        voltageB = 4095;
    }
    
    midiNoteSetChanFrequency(freqA, freqB, freqS);
    dacWrite(0, voltageA);
    dacWrite(1, voltageB);

}

// core1
void midiNoteFreqAdjusting(uint flags)
{
    if(flags & 0x20)        // test
    {
        int amount = flags & 0x04 ? 2 : 1;  // x1
        if(flags & 0x80)    // -
        {
            if(countY - amount >= 20)
            {
                countY = countY - amount;
            }
        }
        else
        {
            if(countY + amount <= 32)
            {
                countY = countY + amount;
            }
        }
        midiNoteReset2();        
    }    
    else if(flags & 0x40)   // sub bass
    {
        if(flags & 0x02)    // St
        {
            int amount = flags & 0x04 ? 3 : 1;  // x1
            if(flags & 0x80)    // -
            {
                int newAmount = freqStAdjustingSub - amount;
                if((newAmount >= -36) && (newAmount + freqCtAdjustingSub/freqFactor >= -36))
                {
                    freqStAdjustingSub = newAmount;
                }
            }
            else
            {
                int newAmount = freqStAdjustingSub + amount;
                if((newAmount <= 36) && (newAmount + freqCtAdjustingSub/freqFactor <= 36))
                {
                    freqStAdjustingSub = newAmount;
                }
            }
        }
        else                // Ct
        {
            int amount = flags & 0x04 ? 10 : 1;
            if(flags & 0x80)    // -
            {
                int newAmount = freqCtAdjustingSub - amount;
                if((newAmount >= -freqFactor) && (freqStAdjustingSub + newAmount/freqFactor >= -36))
                {
                    freqCtAdjustingSub = newAmount;
                }
            }
            else
            {
                int newAmount = freqCtAdjustingSub + amount;
                if((newAmount <= freqFactor) && (freqStAdjustingSub + newAmount/freqFactor <= 36))
                {
                    freqCtAdjustingSub = newAmount;
                }
            }
        }
    }
    else if(flags & 0x01)       // ChanB
    {
        if(flags & 0x02)    // St
        {
            int amount = flags & 0x04 ? 3 : 1;  // x1
            if(flags & 0x80)    // -
            {
                int newAmount = freqStAdjustingChanB - amount;
                if((newAmount >= -36) && (newAmount + freqCtAdjustingChanB/freqFactor >= -36))
                {
                    freqStAdjustingChanB = newAmount;
                }
            }
            else
            {
                int newAmount = freqStAdjustingChanB + amount;
                if((newAmount <= 36) && (newAmount + freqCtAdjustingChanB/freqFactor <= 36))
                {
                    freqStAdjustingChanB = newAmount;
                }
            }
        }
        else                // Ct
        {
            int amount = flags & 0x04 ? 10 : 1;
            if(flags & 0x80)    // -
            {
                int newAmount = freqCtAdjustingChanB - amount;
                if((newAmount >= -freqFactor) && (freqStAdjustingChanB + newAmount/freqFactor >= -36))
                {
                    freqCtAdjustingChanB = newAmount;
                }
            }
            else
            {
                int newAmount = freqCtAdjustingChanB + amount;
                if((newAmount <= freqFactor) && (freqStAdjustingChanB + newAmount/freqFactor <= 36))
                {
                    freqCtAdjustingChanB = newAmount;
                }
            }
        }
    }
    else                // ChanA
    {
        if(flags & 0x02)    // St
        {
            int amount = flags & 0x04 ? 3 : 1;  // x1
            if(flags & 0x80)    // -
            {
                int newAmount = freqStAdjustingChanA - amount;
                if((newAmount >= -36) && (newAmount + freqCtAdjustingChanA/freqFactor >= -36))
                {
                    freqStAdjustingChanA = newAmount;
                }
            }
            else
            {
                int newAmount = freqStAdjustingChanA + amount;
                if((newAmount <= 36) && (newAmount + freqCtAdjustingChanA/freqFactor <= 36))
                {
                    freqStAdjustingChanA = newAmount;
                }
            }
        }
        else                // Ct
        {
            int amount = flags & 0x04 ? 10 : 1;
            if(flags & 0x80)    // -
            {
                int newAmount = freqCtAdjustingChanA - amount;
                if((newAmount >= -freqFactor) && (freqStAdjustingChanA + newAmount/freqFactor >= -36))
                {
                    freqCtAdjustingChanA = newAmount;
                }
            }
            else
            {
                int newAmount = freqCtAdjustingChanA + amount;
                if((newAmount <= freqFactor) && (freqStAdjustingChanA + newAmount/freqFactor <= 36))
                {
                    freqCtAdjustingChanA = newAmount;
                }
            }
        }        
    }
    printf("set freq adjusting (%d:%d), (%d:%d), (%d:%d)\n", 
        freqStAdjustingChanA, freqCtAdjustingChanA, 
        freqStAdjustingChanB, freqCtAdjustingChanB, 
        freqStAdjustingSub, freqCtAdjustingSub);
    midiNoteSetFrequencyFromNote(lastMidiNote);
}

void midiNoteNoteOff(uint note)
{
    gpio_put(gatePin, false);
}

void midiNoteSendTuneNote(int amount)
{
    valA += amount;
    if(valA < 0)
    {
        valA = 0;
    }
    if(valA > 4095)
    {
        valA = 4095;
    }

    queueCore1Item item = {0};
    item.op = 1;
    item.flags = halfA ? 2 : 0; 
    item.value = valA; 
    core1AddItem(&item);
}

void midiNoteSaveMidiNote(void)
{
    unsigned int voltage = (valA & 0x0fff)|(halfA ? 0x2000 : 0x0000);
    midiNoteSetVoltageToMideiNote(lastMidiNote, voltage);
    printf("Last midi note %u, set val %u\n", lastMidiNote, voltage);
}

void midiNoteSetHalf(bool half)
{
    halfA = half;
    printf("Set half: %s\n", halfA ? "true" : "false");
}

void midiNoteSetData(uint16_t num, uint16_t *voltage)
{
    if(num < 1 || num > 16)
    {
        return;
    }
    uint start = (num-1)*8;
    for(int i = 0; i < 8; i++)
    {
        MidiNoteToVoltage[i+start] = voltage[i];
    }
}

void midiNoteGetData(uint16_t num, uint16_t *voltage)
{
    if(num < 1 || num > 16)
    {
        return;
    }
    uint start = (num-1)*8;
    for(int i = 0; i < 8; i++)
    {
        voltage[i] = MidiNoteToVoltage[i+start];
    }
}

void midiNotePrintVoltageTable(void)
{
    // Math.Pow(2, (i - 69) / 12.0) * 440.0
    uint16_t data[8];
    for(int i = 1; i <= 16; i++)
    {
        midiNoteGetData(i, data);
        for(int j = 0; j < 8; j++)
        {
            printf("%d,", data[j]);
        }
        printf("\n");
    }
    printf("================\n");
    printf("f,v\n");
    for(int i = 23; i <= 120; i++)
    {
        float pp = (float)i - 69.0f;
        pp = pp / 12.0f;
        float freq = pow(2, pp) * 440.0f;
        uint voltage = midiNoteGetVoltageFromMideiNote(i);
        printf("%4.4f,%u\n", freq, voltage);
    }
}