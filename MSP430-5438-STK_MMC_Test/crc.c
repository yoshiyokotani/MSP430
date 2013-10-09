#include "crc.h"

unsigned char CalcCRC7(const unsigned char *cmd_seq)
{
  unsigned char i, j;
  unsigned short g;
  unsigned short temp = (unsigned short)cmd_seq[0];
  unsigned short mask;
  
  for (i = 0; i < 5; i++)
  {
    g = 0x8900;         /*(x^7+x^3+1)*x^8*/
    mask = 0x8000;
    temp = (temp << 8) | cmd_seq[i+1];
    for (j = 0; j < 8; j++)
    {
      if (temp & mask)
      {
        temp ^= g;
      }
      mask >>= 1;
      g >>= 1;
    }
  }
  return (unsigned char)(temp & 0x00FF);
}

unsigned short CalcCRC16(const unsigned char *cmd_seq, const unsigned int length)
{
  unsigned int i, j;
  unsigned long g;
  unsigned int temp;
  unsigned long mask;
   
  temp = (unsigned int)cmd_seq[0] << 8;
  temp |= cmd_seq[1];
  temp <<= 8;
  temp |= cmd_seq[2];
  
  for (i = 0; i < (length-3); i++)
  {
    g    = 0x88108000;         /*(x^16+x^12+x^5+1)*x^15*/
    mask = 0x80000000;
    temp = (temp << 8) | cmd_seq[i+3];
    for (j = 0; j < 8; j++)
    {
      if (temp & mask)
      {
        temp ^= g;
      }
      mask >>= 1;
      g >>= 1;
    }
  }
  return (unsigned short)(temp & 0x0000FFFF);
}
