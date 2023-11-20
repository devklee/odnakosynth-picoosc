// See https://aka.ms/new-console-template for more information
using pmiditool;
Midi midi = new Midi();
try
{
    Console.WriteLine("Init midi devices.");
    midi.initMidiDevices();

    OctaveAmountHandler handler = new(midi);

    midi.OctaveMessageReceived += handler.OctaveMessageReceived;

    Console.WriteLine("Waiting for a key, ESC end.");
    ConsoleInput(midi, handler);
    Console.WriteLine("Done");
}
catch (Exception ex)
{
    Console.WriteLine(ex);
}
finally
{
    midi.done();
}

static void ConsoleInput(Midi midi, OctaveAmountHandler handler)
{
    bool half = false;
    bool done = false;
    while (!done)
    {
        if (Console.KeyAvailable)
        {
            ConsoleKeyInfo kinfo =  Console.ReadKey(true);
            switch(kinfo.Key)
            {
                case ConsoleKey.Escape:
                    done = true;
                    break;

                case ConsoleKey.H:
                    if(half)
                    {
                        Console.WriteLine("Choise full range.");
                        half = false;
                    }
                    else
                    {
                        Console.WriteLine("Choise half range.");
                        half = true;
                    }
                    midi.sendCCHalf(half);
                    break;

                case ConsoleKey.Q:
                    Console.WriteLine("A -100");
                    midi.sendCCTuneValue(61);
                    break;
                case ConsoleKey.W:
                    Console.WriteLine("A -10");
                    midi.sendCCTuneValue(62);
                    break;
                case ConsoleKey.E:
                    Console.WriteLine("A -1");
                    midi.sendCCTuneValue(63);
                    break;
                case ConsoleKey.R:
                    Console.WriteLine("A click");
                    midi.sendCCTuneValue(64);
                    break;
                case ConsoleKey.T:
                    Console.WriteLine("A +1");
                    midi.sendCCTuneValue(65);
                    break;
                case ConsoleKey.Z:
                    Console.WriteLine("A +10");
                    midi.sendCCTuneValue(66);
                    break;
                case ConsoleKey.U:
                    Console.WriteLine("A +100");
                    midi.sendCCTuneValue(67);
                    break;

                case ConsoleKey.C:
                    Console.WriteLine("Reset.");
                    midi.sendCCReset();
                    break;
                
                case ConsoleKey.P:
                    Console.WriteLine("Send ping.");
                    midi.Ping();
                    break;

                case ConsoleKey.G:
                    Console.WriteLine("Get frequency.");
                    midi.sendGetFreq(0);
                    break;
                
                case ConsoleKey.N:
                    handler.StartReadOctaveData();
                    break;
                
                case ConsoleKey.M:
                    handler.StopReadOctaveData();
                    break;

                case ConsoleKey.S:
                    handler.SaveDataToFile();
                    break;
                
                case ConsoleKey.D:
                    handler.ReadDataFromFile();
                    break;

                case ConsoleKey.A:
                    handler.SendDataToDevice();
                    break;

            }
        }
        else
        {
            handler.Task();
            Task.Delay(100).Wait();
        }
    }
}
