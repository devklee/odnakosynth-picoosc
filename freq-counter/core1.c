#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "pico/stdlib.h"
#include "pico/util/queue.h"

#include "core1.h"
#include "mpc4822.h"
#include "midi.h"
#include "midinote.h"

#define UART_ID uart0
volatile bool cmdMode = false;
volatile bool cmdShowFrequency = true;
volatile bool cmdProcessFrequency = false;
volatile bool cmdProcessHalfVoltage = true;

//queue
const uint queueLength = 256;
queue_t queue;

// frequency
volatile bool sendFrequencyToPc = false;
volatile uint freqMessageCounter = 0;

void queueTask(void);
void cmdTask(void);

void core1Init(void)
{
    queue_init(&queue, sizeof(queueCore1Item), queueLength);
}

void core1Entry(void)
{
    printf("\n[start core1]  ==========================\n"); 
    while (true) 
    {
        cmdTask();
        queueTask();
    }    
}

bool isCmdMode(void)
{
    return cmdMode;
}

bool isShowFrequency(void)
{
    return !cmdMode && cmdShowFrequency;
}

bool isProcessFrequency(void)
{
    return cmdProcessFrequency;
}

bool isProcessHalfVoltage(void)
{
    return cmdProcessHalfVoltage;
}


bool isSendFrequencyToPc(void)
{
    return sendFrequencyToPc;
}

bool core1AddItem(queueCore1Item *item)
{
    return queue_try_add(&queue, item);
}

void cmdTask(void)
{
    uint8_t ch = 0; 
    if(uart_is_readable(UART_ID))
    {
        ch = uart_getc(UART_ID);
        if(ch == 27)
        {
            cmdMode = !cmdMode;
            if(cmdMode)
            {
                printf("\n>>>command mod on<<<\n");    
            }
            else
            {
                printf("\n>>>command mod off<<<\n");
            }
        }    

        if (uart_is_writable(UART_ID)) 
        {
            //uart_putc(UART_ID, ch);
            //printf(">>>%d<<<\n", (int)ch);
        } 
    }

    if(cmdMode)
    {
        if(ch == 'h') 
        {
            printf(">>>set chanell A high<<<\n");
            queueCore1Item item = {0};
            item.op = 1;
            item.value = 4095;
            core1AddItem(&item);
            printf(">>>set chanell B high<<<\n");
            item.flags = 1; 
            core1AddItem(&item);
        }  
        else if(ch == 'l') 
        {
            printf(">>>set chanell A low<<<\n");
            queueCore1Item item = {0};
            item.op = 1;
            item.value = 0;
            core1AddItem(&item);
            printf(">>>set chanell B low<<<\n");
            item.flags = 1; 
            core1AddItem(&item);
        }  
        else if(ch == 'f')
        {
            cmdProcessHalfVoltage = !cmdProcessHalfVoltage;
            printf(">>>set half voltage: %d<<<\n", cmdProcessHalfVoltage);
        }
        else if(ch == 'p')
        {
            // ping
            printf(">>>send ping<<<\n");
            queueCore1Item item = {0};
            core1AddItem(&item);
        }
        else if(ch == 's')
        {
            cmdShowFrequency = !cmdShowFrequency;
            printf(">>>show frequency: %d<<<\n", cmdShowFrequency);
        }
        else if(ch == 'n')
        {
            cmdProcessFrequency = !cmdProcessFrequency;
            printf(">>>process frequency: %d<<<\n", cmdProcessFrequency);
        }
        else if(ch == 't')
        {
            printf(">>>midi note voltage table<<<\n");
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
        }
    }
}

void queueTask(void)
{
    queueCore1Item item;
    if(!queue_try_remove(&queue, &item))
    {
        return;
    }
 
    switch(item.op)
    {
        case 0:
            midiSendPing();
            break;

        case 1: // send value to dac
            dacWrite(item.flags, item.value);
            break;

        case 2: // not used
            break;

        case 3: // send midi octave table
            midiSendNoteTable(item.value);
            break;

        case 4: // reset
            printf("Reset\n");
            dacWrite(0, 0); // A 0, full
            dacWrite(1, 0); // B 0, full
            midiNoteReset();
           break;

        case 5: // frequency
            sendFrequencyToPc = true;
            freqMessageCounter = item.value;
            break;

        case 6: // send frequency
            if(sendFrequencyToPc)
            {                
                midiSendFreq(freqMessageCounter, item.value);
            }
            sendFrequencyToPc = false;
            break;
    }
}

void cmdPrint(const char *restrict message, ...) 
{
    if (cmdMode)
    {
        return;
    }

    va_list variable_arguments;
    va_start(variable_arguments, message);
    vprintf(message, variable_arguments);
    printf("\n");
    va_end(variable_arguments);
}