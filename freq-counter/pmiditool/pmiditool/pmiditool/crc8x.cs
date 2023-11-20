
namespace pmiditool
{
    internal static class crc8x
    {
        private static byte[] table = new byte[256];
        
        // x8 + x7 + x6 + x4 + x2 + 1
        const byte poly = 0xd5;

        static crc8x()
        {
            for (int i = 0; i < 256; ++i)
            {
                int temp = i;
                for (int j = 0; j < 8; ++j)
                {
                    if ((temp & 0x80) != 0)
                    {
                        temp = (temp << 1) ^ poly;
                    }
                    else
                    {
                        temp <<= 1;
                    }
                }
                table[i] = (byte)temp;
            }
        }

        public static uint ComputeChecksum(int length, byte[] data)
        {
            uint crc = 0; 
            for (int i = 0; i < length; i++)
            {
                crc = table[data[i] ^ crc];
            }
            return crc;
        }
    }
}
