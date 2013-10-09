#ifndef _CRC_H_
#define _CRC_H

unsigned char CalcCRC7(const unsigned char *cmd_seq);
unsigned short CalcCRC16(const unsigned char *cmd_seq, const unsigned int length);

#endif /*_CRC_H_*/