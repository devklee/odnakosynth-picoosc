using System.Diagnostics;

namespace pmiditool
{
    internal class FrequencyHandler
    {
        private readonly Midi midi;
        private readonly MidiNotenData[] midiNotenData;
        
        uint taskStep = 0;
        uint sendMessageCounter = 1;
        bool waitForFrequency = false;
        uint messageCounter = 0;
        uint messageFrequency = 0;
        uint prevSendMessageFrequency = 0;
        uint readFrequencyCounter = 0;
        uint[] frequency = new uint[4096];
        uint freqVoltage = 0;
        long readFrequencyElapsed = 0;

        //TODO test
        uint startVoltage = 0;

        Stopwatch watch = new Stopwatch();

        public FrequencyHandler(Midi midi)
        {
            this.midi = midi;

            midiNotenData = new MidiNotenData[128];
            for (int i = 0; i < midiNotenData.Length; i++) 
            {
                double noteFreq = Math.Pow(2, (i - 69) / 12.0) * 440.0;
                midiNotenData[i] = new MidiNotenData(noteFreq);
            }
        }

        public void FrequencyMessageReceived(object sender, FrequencyMessageEventArgs e)
        {
            if (waitForFrequency) 
            { 
                if(sendMessageCounter != e.MessageCount) 
                {
                    Console.WriteLine($"Invalid message counter {e.MessageCount}, {sendMessageCounter}");
                }
                else
                {
                    messageCounter = e.MessageCount;
                    messageFrequency = e.Frequency;
                    waitForFrequency = false;
                }
            }
            else
            {
                Console.WriteLine($"Counter {e.MessageCount}, frequency {e.Frequency}");
            }
        }

        public void SaveFreqDataToFile()
        {
            string path = "freqdata.cvs";
            if (File.Exists(path))
            {
                File.Delete(path);
            }
            using FileStream fs = new(path, FileMode.Append);
            using StreamWriter sw = new(fs);
            
            sw.WriteLine("a;f");
            for(int i = 0; i < frequency.Length; i++) 
            {
                double f = (double)frequency[i] / 1000.0;
                sw.WriteLine($"{i};{f.ToString("0.000").Replace(",", ".")}");
            }
            sw.Close();
            fs.Close();
            Console.WriteLine($"Frequency data saved to the {path} file.");

            string path1 = "freqdatamidi.h";
            if (File.Exists(path1))
            {
                File.Delete(path1);
            }
            using FileStream fs1 = new(path1, FileMode.Append);
            using StreamWriter sw1 = new(fs1);

            sw1.WriteLine("unsigned int MidiNoteToVoltage[] =");
            sw1.WriteLine("{");
            for (int i = 0; i < midiNotenData.Length; i++)
            {
                long f = Convert.ToInt32(Math.Ceiling(midiNotenData[i].FreqNote * 1000.0));
                sw1.Write($"{f}{(i == 127 ? "" : ",")}");
                if((i+1)%12 == 0)
                {
                    sw1.WriteLine();
                }
            }
            sw1.WriteLine();
            sw1.WriteLine("};");
            sw1.Close();
            fs1.Close();
            Console.WriteLine($"Midi note data saved to the {path1} file.");
        }
        
        public void ReadFreqDataFromFile()
        {
            string path = "freqdate.cvs";
            if (!File.Exists(path))
            {
                Console.WriteLine($"File {path} not found.");
                return;
            }
            using FileStream fs = new(path, FileMode.Open);
            using StreamReader sr = new(fs);

            string? l = sr.ReadLine();  // read head line
            l = sr.ReadLine();
            while (l != null)
            {
                string[] strings = l.Split(new char[] { ';'});
                if(strings.Length == 2) 
                { 
                    uint freqVoltage = uint.Parse(strings[0]);
                    if(freqVoltage >= 0 && freqVoltage <= 4095) 
                    { 
                        double freq = double.Parse(strings[1].Replace(".",""))/1000.0;
                        if(freq > 0.0 && freq < 13000.0)
                        {
                            frequency[freqVoltage] = Convert.ToUInt32(Math.Ceiling(freq * 1000.0));
                            int m = Convert.ToInt32(Math.Ceiling(69.0 + 12 * Math.Log2(freq / 440.0)));
                            if (m < 0 || m > 127)
                            {
                                Console.WriteLine($"Invalid midi note index {m}");
                            }
                            else
                            {
                                if (midiNotenData[m].Freq == 0.0)
                                {
                                    midiNotenData[m].Freq = freq;
                                    midiNotenData[m].Delta = Math.Abs(midiNotenData[m].Freq - midiNotenData[m].FreqNote) / midiNotenData[m].FreqNote;
                                    midiNotenData[m].Amount = freqVoltage;
                                }
                                else
                                {
                                    double delta = Math.Abs(freq - midiNotenData[m].FreqNote) / midiNotenData[m].FreqNote;
                                    if (delta < midiNotenData[m].Delta)
                                    {
                                        midiNotenData[m].Freq = freq;
                                        midiNotenData[m].Delta = delta;
                                        midiNotenData[m].Amount = freqVoltage;
                                    }
                                }
                            }
                        }
                    }
                }
                
                l = sr.ReadLine();
            }

            sr.Close();
            fs.Close();
            Console.WriteLine($"Frequency data readed from the {path} file.");
        }

        public void StartReadFrequency()
        {
            if (taskStep == 0)
            {
                Console.WriteLine("Start frequency read task.");
                taskStep = 1;
            }
            else
            {
                Console.WriteLine("Frequency read task already started.");
            }
        }

        public void StopReadFrequency()
        {
            taskStep = 99;
        }


        public void Task()
        { 
            switch(taskStep)
            {
                case 0:     // wait for a start                    
                    break;

                case 1:     // start
                    Console.WriteLine("Start get frequency table.");
                    midi.sendCCReset();
                    watch.Restart();
                    sendMessageCounter = 1;
                    Array.Clear(frequency, 0, frequency.Length);
                    freqVoltage = startVoltage;
                    taskStep = 2;
                    break;

                case 2:     // init get frequency
                    waitForFrequency = false;
                    readFrequencyCounter = 0;
                    prevSendMessageFrequency = 0;
                    midi.sendVoltage(true, false, freqVoltage);     // chanA, full
                    taskStep = 3;
                    break;

                case 3:     // send SysEx message                    
                    sendMessageCounter++;
                    if(sendMessageCounter > 9999)
                    {
                        sendMessageCounter = 1;
                    }
                    waitForFrequency = true;
                    midi.sendGetFreq(sendMessageCounter);
                    readFrequencyElapsed = watch.ElapsedMilliseconds;
                    taskStep = 4;
                    break;

                case 4:     // read frequency
                    if (waitForFrequency)
                    {
                        long elapsed =  watch.ElapsedMilliseconds - readFrequencyElapsed;
                        if(elapsed > 5000)
                        {
                            Console.WriteLine($"Read frequency timeout, set frequency for {freqVoltage} to 0.");
                            messageFrequency = 0;
                            taskStep = 5;
                        }
                        break;
                    }

                    if(readFrequencyCounter == 0)
                    {
                        prevSendMessageFrequency = messageFrequency;
                        readFrequencyCounter++;
                        taskStep = 3;
                        break;
                    }
                    readFrequencyCounter++;
                    if(readFrequencyCounter > 10)
                    {
                        Console.WriteLine($"Too much frequency read attempts, set frequency for {freqVoltage} to {messageFrequency}.");
                        prevSendMessageFrequency = messageFrequency;
                        taskStep = 5;
                        break;
                    }
                    if (messageFrequency != 0 && prevSendMessageFrequency == 0)
                    {
                        prevSendMessageFrequency = messageFrequency;
                        taskStep = 3;
                        break;
                    }
                    if (prevSendMessageFrequency == 0)
                    {
                        messageFrequency = 0;
                        taskStep = 5;
                        break;
                    }
                    double percDelta =  (double)Math.Abs(messageFrequency - prevSendMessageFrequency) / prevSendMessageFrequency;
                    prevSendMessageFrequency = messageFrequency;
                    if (percDelta > 0.01)
                    {                        
                        taskStep = 3;
                        break;
                    }
                    taskStep = 5;
                    break;

                case 5:     // save frequency
                    double f = (double)messageFrequency / 1000.0;
                    if(f > 0.0)
                    {
                        int m = Convert.ToInt32(Math.Ceiling(69.0 + 12 * Math.Log2(f / 440.0)));
                        if (m < 0 || m > 127)
                        {
                            Console.WriteLine($"Invalid midi note index {m}");
                        }
                        else
                        {
                            if (midiNotenData[m].Freq == 0.0)
                            {
                                midiNotenData[m].Freq = f;
                                midiNotenData[m].Delta = Math.Abs(midiNotenData[m].Freq - midiNotenData[m].FreqNote) / midiNotenData[m].FreqNote;
                                midiNotenData[m].Amount = freqVoltage;
                            }
                            else
                            {
                                double delta = Math.Abs(f - midiNotenData[m].FreqNote) / midiNotenData[m].FreqNote;
                                if(delta < midiNotenData[m].Delta)
                                {
                                    midiNotenData[m].Freq = f;
                                    midiNotenData[m].Delta = delta;
                                    midiNotenData[m].Amount = freqVoltage;
                                }
                            }
                        }
                    }

                    frequency[freqVoltage] = messageFrequency;
                    freqVoltage++;
                    if(freqVoltage > 4095)
                    {
                        Console.WriteLine($"Readed frequency complitly");
                        taskStep = 99;
                        break;
                    }
                    else
                    {
                        Console.WriteLine($"Readed frequency {messageFrequency} complitly vor voltage {freqVoltage}");
                    }
                    taskStep = 2;
                    break;
                
                case 99:
                    watch.Stop();
                    Console.WriteLine($"Stop. Time elapsed {watch.Elapsed}");
                    taskStep = 0;
                    break;

                default:
                    Console.WriteLine($"Invalid task step: {taskStep}");
                    break;
            }
        }

    }
}
