#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"

#include "midi.h"
#include "midinote.h"
#include "core1.h"


unsigned int lastMidiNote = 0;

// dac
int valA = 0;
bool halfA = false;
int valB = 0;
bool halfB = false;

unsigned int midiNoteGetVoltageFromMideiNote(uint16_t note);
void midiNoteSetVoltageToMideiNote(uint16_t note, uint16_t voltage);

#define MAX_MIDI_NOTE 127
static uint16_t MidiNoteToVoltage[128] = 
{ 
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static float FreqDelta[128] = 
{ 
    0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,
    0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,
    0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,
    0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,
    0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,
    0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,
    0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,
    0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.,0.
};

//  Math.Pow(2, (i - 69) / 12.0) * 440.0
//  xxxxx[.]nnn
static uint32_t MidiNoteFrequency[128] = 
{ 
    8176,8662,9178,9723,10301,10914,11563,12250,12979,13750,14568,15434,
    16352,17324,18355,19446,20602,21827,23125,24500,25957,27500,29136,30868,
    32704,34648,36709,38891,41204,43654,46250,49000,51914,55000,58271,61736,
    65407,69296,73417,77782,82407,87308,92499,97999,103827,110000,116541,123471,
    130813,138592,146833,155564,164814,174615,184998,195998,207653,220000,233082,246942,
    261626,277183,293665,311127,329628,349229,369995,391996,415305,440000,466164,493884,
    523252,554366,587330,622254,659256,698457,739989,783991,830610,880000,932328,987767,
    1046503,1108731,1174660,1244508,1318511,1396913,1479978,1567982,1661219,1760000,1864656,1975534,
    2093005,2217462,2349319,2489016,2637021,2793826,2959956,3135964,3322438,3520000,3729311,3951067,
    4186010,4434923,4698637,4978032,5274041,5587652,5919911,6271927,6644876,7040000,7458621,7902133,
    8372019,8869845,9397273,9956064,10548082,11175304,11839822,12543854
};

void midiNoteReset(void)
{
    valA = 0;
    halfA = false;
    valB = 0;
    halfB = false;

    for(int i = 0; i <= MAX_MIDI_NOTE; i++)
    {
        FreqDelta[i] = 0.;
    }
}

//  freq: xxxxx[.]nnn
//  voltage: 0..4095
void midiNoteSetNoteVoltage(uint32_t freq, uint16_t voltage)
{
    int m = (int)ceil(69.0 + 12 * log2(freq / 440000.0));
    if (m < 0 || m > 127)
    {
        printf("Invalid midi note index %d\n", m);
        return;
    }
   
    float delta = (float)abs(freq-MidiNoteFrequency[m])/(float)MidiNoteFrequency[m];
    if(FreqDelta[m] == 0.)
    {
        FreqDelta[m] = delta;
        MidiNoteToVoltage[m] = voltage;
        if(isShowFrequency())
        {
            printf("Set midi note %d, voltage %d, frequency %u\n", m, voltage, freq);
        }
    }
    else
    {
        if(delta < FreqDelta[m])
        {
            FreqDelta[m] = delta;
            MidiNoteToVoltage[m] = voltage;
            if(isShowFrequency())
            {
                printf("Set midi note %d, voltage %d, frequency %u\n", m, voltage, freq);
            }
        }
    }
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
}

void midiNoteSendMidiNote(uint16_t note)
{
    lastMidiNote = note;
    queueCore1Item item = {0};
    item.op = 1;        
    unsigned int voltage = midiNoteGetVoltageFromMideiNote(note);
    item.flags = (voltage & 0xf000) >> 12;
    item.value = voltage & 0x0fff; 
    core1AddItem(&item);
    if(item.flags & 1)
    {
        valB = item.value;
        halfB = item.flags & 2;
    }
    else
    {
        valA = item.value;
        halfA = item.flags & 2;
    }    
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