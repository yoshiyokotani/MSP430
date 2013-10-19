#ifndef _MMC_RESPONSE_H_
#define _MMC_RESPONSE_H_

#define RESPONSE_BUFFER_LENGTH          32
#define CMD_0                           0x0000
#define CMD_8                           0x0008
#define CMD_9                           0x0009
#define CMD_10                          0x000A
#define CMD_12                          0x000C
#define CMD_13                          0x000D
#define CMD_16                          0x0010
#define CMD_17                          0x0011
#define CMD_18                          0x0012
#define CMD_24                          0x0018
#define CMD_25                          0x0019
#define CMD_32                          0x0020
#define CMD_33                          0x0021
#define CMD_38                          0x0026
#define CMD_55                          0x0037
#define CMD_58                          0x003A
#define ACMD_41                         0x0129

#define R1_COMPLETE                     0x0000
#define R1_IN_IDLE_STATE                0x0001
#define R1_ERASE_RESET                  0x0002
#define R1_ILLEGAL_COMMAND              0x0004
#define R1_COM_CRC_ERROR                0x0008
#define R1_ERASE_SEQUENCE_ERROR         0x0010
#define R1_ADDRESS_ERROR                0x0020
#define R1_PARAMETER_ERROR              0x0040
#define R1_BUSY                         0x0080
#define R1_ALL_ZEROS                    0x0100

#define R2_CARD_IS_LOCKED               0x0080
#define R2_WP_ERASE_SKIP                0x0100
#define R2_ERROR                        0x0200
#define R2_CC_ERROR                     0x0400
#define R2_CARD_ECC_FAILED              0x0800
#define R2_WP_VIOLATION                 0x1000
#define R2_ERASE_PARAM                  0x2000
#define R2_OUT_OF_RANGE                 0x4000

#define DATA_ACCEPTED                   0x0010
#define DATA_CRC_ERROR                  0x0101
#define DATA_WRITE_ERROR                0x0110

#define TOKEN_START_OK                  0x0000
#define TOKEN_START_NOK                 0x0001

extern unsigned char mmc_read_write_buffer[];

typedef struct CSDRegister 
{
  unsigned char         CsdStructure;
  unsigned short        ReadBlockLength;
  unsigned long         NumBlocks;
  unsigned char         EraseSingleBlockEnable;
  unsigned short        ErasableBlocks;
  unsigned short        WriteBlockLength;
  unsigned char         TemporaryWriteProtection;
}CSDRegister;

void mmcInitBuffers(void);
unsigned char mmcWriteReadByte(const unsigned char inputByte);
void mmcSendCMD(const short cmd, unsigned long data);
void mmcReadResponse(const unsigned int numBytes);
void mmcReadBuffer(const unsigned int numBytes);
void mmcWriteBuffer(const unsigned int numBytes);
unsigned short mmcInterpretR1(const unsigned char response);
unsigned short mmcGetR1Response(const int searchLength);
unsigned short mmcGetR1bResponse(void);
unsigned short mmcGetR2Response(void);
unsigned char mmcGetDataResponseToken(unsigned char *isBusy);
unsigned char mmcGetStartBlockToken(int *start_index);
unsigned short mmcGetR3orR7Response(unsigned long *ret_data);
unsigned char mmcGetCardCapacityStatus(unsigned long response_data);
unsigned char mmcGetPowerUpStatus(unsigned long response_data);
unsigned short mmcGetOperationVoltageRange(unsigned long response_data);
unsigned short mmcReadCSDRegister(CSDRegister *csdReg);


#endif /*_MMC_RESPONSE_H_*/