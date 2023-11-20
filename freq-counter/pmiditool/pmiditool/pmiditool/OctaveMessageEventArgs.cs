namespace pmiditool
{
    internal class OctaveMessageEventArgs : EventArgs
    {
        public uint[] NoteAmount { get; private set; }
        public uint Number { get; private set; }
        public OctaveMessageEventArgs(uint number)
        {
            Number = number;
            NoteAmount = new uint[8];
        }
    }
}
