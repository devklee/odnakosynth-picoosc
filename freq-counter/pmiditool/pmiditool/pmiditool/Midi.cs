using NAudio.Midi;
using System;
using System.Collections.Generic;

namespace pmiditool
{
    internal class Midi
    {
        MidiIn? midiIn = null;
        MidiOut? midiOut = null;
        string picoBoaradName = "Pico Frequency Counter Board";
        int midiInNumber = -1;
        int midiOutNumber = -1;

        public event EventHandler<FrequencyMessageEventArgs>? FrequencyMessageReceived;
        public event EventHandler<OctaveMessageEventArgs>? OctaveMessageReceived;

        public void initMidiDevices()
        {
            for (int device = 0; device < MidiIn.NumberOfDevices; device++)
            {
                if(picoBoaradName.Equals(MidiIn.DeviceInfo(device).ProductName, StringComparison.OrdinalIgnoreCase))
                {
                    midiInNumber = device; 
                    Console.WriteLine($"Midi in {device}: {MidiIn.DeviceInfo(device).ProductName}");
                }
            }
            
            for (int device = 0; device < MidiOut.NumberOfDevices; device++)
            {
                if (picoBoaradName.Equals(MidiOut.DeviceInfo(device).ProductName, StringComparison.OrdinalIgnoreCase))
                {
                    midiOutNumber = device;
                    Console.WriteLine($"Midi out {device}: {MidiOut.DeviceInfo(device).ProductName}");
                }
            }

            if (midiInNumber >= 0)
            {
                midiIn = new MidiIn(midiInNumber);
                midiIn.CreateSysexBuffers(512, 2);
                midiIn.SysexMessageReceived += midiInSysexMessageReceived;
                midiIn.ErrorReceived += midiInErrorReceived;
                midiIn.Start();
                Console.WriteLine("Midi reciver startet.");
            }

            if (midiOutNumber >= 0)
            {
                midiOut = new MidiOut(midiOutNumber);
            }
        }

        public void done()
        {
            midiIn?.Stop();
            midiIn?.Dispose();
            midiOut?.Dispose();
        }

        public void Ping()
        {
            midiOut?.SendBuffer(new byte[] { 0xf0, 0x7d, 0x01, 0x00, 0xf7 });
        }

        void midiInErrorReceived(object sender, MidiInMessageEventArgs e)
        {
            Console.WriteLine($"Time {e.Timestamp} Message {e.RawMessage:x8} Event {e.MidiEvent}");
        }

        void midiInSysexMessageReceived(object sender, MidiInSysexMessageEventArgs e)
        {
            //Console.WriteLine($"Sysex message length: {e.SysexBytes.Length}");
            logMidiEvent(e.SysexBytes, (uint)e.SysexBytes.Length);
            uint dataLen = 0;
            bool dataOK = false;

            if (e.SysexBytes.Length >= 4)
            {
                for (int i = 0; i < e.SysexBytes.Length; i++)
                {
                    if (i == 0)
                    {
                        if (e.SysexBytes[i] != 0xf0) break;
                    }
                    else if (i == 1)
                    {
                        if (e.SysexBytes[i] != 0x7d) break;
                    }
                    else if (i == 2)
                    {
                        if (e.SysexBytes[i] != 0x01) break;
                    }
                    else
                    {
                        if (e.SysexBytes[i] == 0xf7)
                        {
                            dataOK = true;
                            break;
                        }
                        dataLen++;
                    }
                }
            }

            if (!dataOK || dataLen == 0)
            {
                Console.WriteLine("Missing SysEx format!");
                return;
            }

            uint posData = 4;
            dataLen--;
            switch (e.SysexBytes[3])
            {
                case 0:             // ping
                    Console.WriteLine("Ping");
                    break;

                case 2:             // frequency
                    // f0 7d 01 02 ee cc cc nn nn nn nn f7
                    if (dataLen != 7) break;
                    if (this.FrequencyMessageReceived == null) break;
                    byte[] dist = new byte[8];
                    if (decode8in7(e.SysexBytes, posData, 7, dist) != 6) break;
                    uint count = bcdToDecimal(dist, 0, 2);
                    uint freq = bcdToDecimal(dist, 2, 4);
                    this.FrequencyMessageReceived(this, new FrequencyMessageEventArgs(count, freq));
                    break;

                case 4:             // recive midi octave table 
                    /*
                        f0 7d 01 03 ee oo nn nn nn nn ... dd f7 (8in7)
                    */
                    if (this.OctaveMessageReceived == null) break;
                    if (dataLen != 30) break;

                    // num + data + crc
                    // 1 + 8*3 + 1 = 26
                    byte[] valBcd = new byte[26];
                    if (decode8in7(e.SysexBytes, posData, 30, valBcd) != 26) break;
                    uint crcMessage = valBcd[25]; 
                    uint crc = crc8x.ComputeChecksum(25, valBcd);
                    if (crc != crcMessage)
                    {
                        Console.WriteLine($"Wrong crc sum {crc} != {crcMessage}.");
                        break;
                    }
                    uint num = bcdToDecimal(valBcd, 0, 1);
                    OctaveMessageEventArgs octave = new OctaveMessageEventArgs(num);
                    for (uint i = 0; i < 8; i++)
                    {
                        octave.NoteAmount[i] = bcdToDecimal(valBcd, 1 + i * 3, 3);
                    }
                    this.OctaveMessageReceived(this, octave);
                    break;

                default:
                    Console.WriteLine($"Unknown SysEx message: {e.SysexBytes[3]}");
                    break;
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

        public uint decode8in7(byte[] src, uint srcPos, uint srcLen, byte[] dist)
        {
            uint l = 0;
            for (uint i = 0; i < srcLen; i += 8)
            {
                byte shift = 1;
                for (uint k = 0; k < 7 && k + i + 1 < srcLen; k++)
                {
                    dist[l] = src[srcPos+k + i + 1] ;
                    if( (src[srcPos+i] & shift) == shift)
                    {
                        dist[l] |= 0x80;
                    }
                    
                    shift = (byte)(shift << 1);
                    l++;
                }
            }
            return l;
        }
        
        public uint encode8in7(byte[] src, uint srcLen, byte[] dist)
        {
            uint l = 0;
            for (uint i = 0; i < srcLen; i += 7)
            {
                byte shift = 1;
                uint flags = l;
                dist[flags] = 0;
                l++;
                for (uint k = 0; k < 7 && k + i < srcLen; k++)
                {
                    dist[l] = (byte)(src[k + i] & 0x7f);
                    if((src[k + i] & 0x80) == 0x80)
                    {
                        dist[flags] |= shift;
                    }
                    shift = (byte)(shift << 1);
                    l++;
                }
            }
            return l;
        }

        void decimalToBcd(uint value, byte[] buf, uint lenghtBcd, uint bufPos = 0)
        {            
            int l = (int)(lenghtBcd - 1);
            while (l >= 0)
            {
                uint v = value % 10;
                value /= 10;
                v += (value % 10) << 4;                
                value /= 10;
                buf[l+ bufPos] = (byte)v;
                l--;
            }
        }

        uint bcdToDecimal(byte[] buf, uint bufPos, uint lenghtBcd)
        {
            uint value = 0;
            int l = (int)(lenghtBcd - 1);
            uint b = 1;
            while (l >= 0)
            {
                value += ((uint)buf[bufPos+l] & 0x0f) * b;
                b *= 10;
                value += (((uint)buf[bufPos+l] & 0xf0) >> 4) * b;
                b *= 10;
                l--;
            }

            return value;
        }
        
        void logMidiEvent(byte[] buf, uint lenght) 
        {
            if (buf[0] == 0xf0)
            {
                uint count = 0;
                for (uint i = 0; i < lenght; i++)
                {
                    if (buf[i] == 0xf7)
                    {
                        count = i + 1;
                        break;
                    }
                }
                Console.Write($"SysEx  {count}  ");
                for (int i = 0; i < count; i++)
                {
                    Console.Write($"{buf[i]:x2} ");
                }
                Console.WriteLine();
            }
        }

        public bool sendVoltage(bool chanA, bool half, uint voltage)
        {
            if(voltage > 4095)
            {
                Console.WriteLine($"Invalid voltage value {voltage}.");
                return false;
            }

            byte[] bcdVoltage = new byte[2];
            decimalToBcd(voltage, bcdVoltage, 2);

            /*                
               ff vv vv
               flag
                  voltage in BCD ==> 0..4096
            */

            byte[] message = new byte[3];
            message[0] = 0;
            if(!chanA) { message[0] |= 1; }
            if(half) { message[0] |= 2; }
            message[1] = bcdVoltage[0];
            message[2] = bcdVoltage[1];

            // ee bb bb bb (8in7)
            byte[] message2 = new byte[4];
            encode8in7(message, 3, message2);

            byte[] sysex = new byte[] 
            { 
                0xf0, 0x7d, 0x01, 0x01,
                message2[0], message2[1],
                message2[2], message2[3],
                0xf7
            };

            midiOut?.SendBuffer(sysex);
            return true;
        }

        public void sendCCTuneValue(int mal)
        {
            byte[] message = { 0xb0, 0x00, 0x00};
            if (mal == 61)      
            {
                // -100
                message[1] = (byte)113;
                message[2] = (byte)62;
            }
            else if (mal == 62)
            {
                // -10
                message[1] = (byte)114;
                message[2] = (byte)62;
            }
            else if (mal == 63)
            {
                // -1
                message[1] = (byte)114;
                message[2] = (byte)63;
            }
            else if (mal == 64)
            {
                // Enter
                message[1] = (byte)115;
                message[2] = (byte)127;
            }
            else if (mal == 65)
            {
                // +1
                message[1] = (byte)114;
                message[2] = (byte)65;
            }
            else if (mal == 66)
            {
                // +10
                message[1] = (byte)114;
                message[2] = (byte)66;
            }
            else if (mal == 67)
            {
                // +100
                message[1] = (byte)113;
                message[2] = (byte)66;
            }

            midiOut?.SendBuffer(message);
        }

        public void sendCCHalf(bool half)
        {
            byte[] message = { 0xb0, 0x70, (byte)(half ? 125 : 2) };  
            midiOut?.SendBuffer(message);
        }

        public void sendCCReset()
        {
            //Reset All Controllers
            byte[] message = { 0xb0, 0x79, 0x00 };
            midiOut?.SendBuffer(message);
        }

        public void sendGetFreq(uint freqMessageCounter)
        {
            byte[] bcdCouner = new byte[2];
            decimalToBcd(freqMessageCounter, bcdCouner, 2);

            // ee cc cc  (8in7)
            byte[] message = new byte[4];
            encode8in7(bcdCouner, 2, message);

            byte[] sysex = new byte[]
            {
                0xf0, 0x7d, 0x01, 0x02,
                message[0], message[1],
                message[2],
                0xf7
            };

            midiOut?.SendBuffer(sysex);
        }

        public void sendGetNoteOctave(uint num)
        {
            if(num > 16)
            {
                Console.WriteLine($"Wrong octave data number {num}.");
                return;
            }

            byte[] sysex = new byte[]
            {
                0xf0, 0x7d, 0x01, 0x03,
                (byte) num,
                0xf7
            };
            midiOut?.SendBuffer(sysex);
        }

        public void sendSetNoteOctave(uint num, uint[] data)
        {
            if (num > 16)
            {
                Console.WriteLine($"Wrong octave data number {num}.");
                return;
            }

            // num + data + crc
            // 1 + 8*3 + 1 = 26
            byte[] valBcd = new byte[26];
            decimalToBcd(num, valBcd, 1);
            for (uint i = 0; i < 8; i++)
            {
                decimalToBcd(data[i], valBcd, 3, 1 + i * 3);
            }
            byte crc = (byte)crc8x.ComputeChecksum(25, valBcd);
            valBcd[25] = crc;

            // 26 / 7 => +4 byte for bits: 26 + 4 = 30   
            byte[] val8in7 = new byte[30];
            encode8in7(valBcd, 26, val8in7);
            byte[] sysex = new byte[35]; 
            sysex[0] = 0xf0;
            sysex[1] = 0x7d;
            sysex[2] = 0x01;
            sysex[3] = 0x04;
            for (int i = 0; i< 30; i++)
            {
                sysex[i + 4] = val8in7[i];
            }
            sysex[34] = 0xf7;

            midiOut?.SendBuffer(sysex);
        }

    }
}
