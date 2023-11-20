namespace pmiditool
{
    internal class MidiNotenData
    {       
        public MidiNotenData(double freqNote)
        {
            FreqNote = freqNote;
        }

        public double FreqNote { get; private set; }
        public double Freq { get; set; } = 0.0;
        public double Delta { get; set; } = 0.0;
        public uint Amount { get; set; } = 0;
    }
}
