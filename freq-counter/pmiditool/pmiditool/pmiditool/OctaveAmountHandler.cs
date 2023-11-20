using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Xml.Linq;

namespace pmiditool
{
    internal class OctaveAmountHandler
    {
        private readonly Midi midi;
        private readonly uint[] midiNotenAmount = new uint[132];

        uint taskStep = 0;
        uint octaveDataCounter = 1;
        bool waitForOctave = false;
        long readElapsed = 0;

        Stopwatch watch = new Stopwatch();

        public OctaveAmountHandler(Midi midi)
        {
            this.midi = midi;
        }

        public void OctaveMessageReceived(object sender, OctaveMessageEventArgs e)
        {
            if (waitForOctave)
            {
                Console.WriteLine($"Recived octave {e.Number} data");
                if(e.Number > 0 && e.Number <= 16)
                {
                    uint start = (e.Number-1) * 8;
                    for (int i = 0; i < 8; i++)
                    {
                        midiNotenAmount[start + i] = e.NoteAmount[i];
                    }
                }
                waitForOctave = false;
            }
        }

        public void SaveDataToFile()
        {
            string path = "octavedata.json";
            if (File.Exists(path))
            {
                File.Delete(path);
            }
            using FileStream fs = new(path, FileMode.Append);
            JsonSerializer.Serialize(fs, midiNotenAmount);
            fs.Close();
            Console.WriteLine($"Octave data saved to the {path} file.");
        }

        public void ReadDataFromFile()
        {
            string path = "octavedata.json";
            if (!File.Exists(path))
            {
                Console.WriteLine($"File {path} not found.");
                return;
            }
            using FileStream fs = new(path, FileMode.Open);
            uint[]? data = JsonSerializer.Deserialize<uint[]>(fs);
            fs.Close();

            if (data == null || data.Length != 132)
            {
                Console.WriteLine($"Invalid octave data in the {path} file.");
            }
            else
            {
                for (int i = 0;i < data.Length;i++) 
                {
                    midiNotenAmount[i] = data[i];
                }
                Console.WriteLine($"Octave data readed from the {path} file.");
            }
        }

        public void SendDataToDevice() 
        {
            for(uint i = 0; i < 16; i++)
            {
                uint start = i * 8;
                uint[] data = new uint[8];
                for(int j = 0; j < 8; j++)
                {
                    data[j] = midiNotenAmount[start + j];
                }
                midi.sendSetNoteOctave(i + 1, data);
            }
        }
        
        public void StartReadOctaveData()
        {
            if (taskStep == 0)
            {
                Console.WriteLine("Start octave data read task.");
                taskStep = 1;
            }
            else
            {
                Console.WriteLine("Octave data read task already started.");
            }
        }

        public void StopReadOctaveData()
        {
            taskStep = 99;
        }


        public void Task()
        {
            switch (taskStep)
            {
                case 0:     // wait for a start                    
                    break;

                case 1:     // start
                    Console.WriteLine("Start read octave data table.");
                    watch.Restart();
                    taskStep = 2;
                    break;

                case 2:     // init get octave data
                    waitForOctave = false;
                    octaveDataCounter = 1;
                    taskStep = 3;
                    break;

                case 3:     // send SysEx message                                        
                    waitForOctave = true;
                    midi.sendGetNoteOctave(octaveDataCounter);
                    readElapsed = watch.ElapsedMilliseconds;
                    taskStep = 4;
                    break;

                case 4:     // wait for a octave data
                    if (waitForOctave)
                    {
                        long elapsed = watch.ElapsedMilliseconds - readElapsed;
                        if (elapsed > 5000)
                        {
                            Console.WriteLine($"Read timeout for a octave data {octaveDataCounter}, stop.");
                            taskStep = 99;
                        }
                    }
                    else
                    {
                        taskStep = 5;
                    }
                    break;

                case 5:     // get next octave
                    octaveDataCounter++;
                    if (octaveDataCounter > 16)
                    {
                        Console.WriteLine("Readed octave data complitly.");
                        taskStep = 99;
                        break;
                    }
                    Console.WriteLine($"read octave {octaveDataCounter} data.");
                    taskStep = 3;
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
