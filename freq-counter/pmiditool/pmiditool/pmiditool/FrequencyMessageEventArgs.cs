namespace pmiditool
{
    internal class FrequencyMessageEventArgs : EventArgs
    {
        public uint MessageCount { get; private set; }
        public uint Frequency { get; private set; }
        public FrequencyMessageEventArgs(uint messageCount, uint frequency)
        {
            MessageCount = messageCount;
            Frequency = frequency;
        }
    }
}
