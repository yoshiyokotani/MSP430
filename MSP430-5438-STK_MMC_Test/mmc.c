#include "io430.h"
#include "mmc.h"

volatile unsigned char mmc_read_write_buffer[512];
volatile unsigned char mmc_response_buffer[RESPONSE_BUFFER_LENGTH] = {0};

void ReadResponse(const unsigned int numBytes)
{
  unsigned int i = 0;
  
  while(i < numBytes)
  {
    mmc_response_buffer[i++]= mmcSendByte(0xFF);
  }
}
  
void ReadBuffer(const unsigned int numBytes)
{
  unsigned int i = 0;
  
  while(i < numBytes)
  {
    mmc_read_write_buffer[i++]= mmcSendByte(0xFF);
  }
}

unsigned short InterpretR1(unsigned char response)
{
  unsigned short R1_response = 0xFFFF;

  switch (response)
  {
    case 0x00:
      R1_response = (unsigned short)R1_COMPLETE;
      break;
    case 0x01:
      R1_response = (unsigned short)R1_IN_IDLE_STATE;
      break;
    case 0x02:
      R1_response = (unsigned short)R1_ERASE_RESET;
      break;
     case 0x04:
      R1_response = (unsigned short)R1_ILLEGAL_COMMAND;
      break;
    case 0x08:
      R1_response = (unsigned short)R1_COM_CRC_ERROR;
      break;
    case 0x10:
      R1_response = (unsigned short)R1_ERASE_SEQUENCE_ERROR;
      break;
    case 0x20:
      R1_response = (unsigned short)R1_ADDRESS_ERROR;
      break;
    case 0x40:
      R1_response = (unsigned short)R1_PARAMETER_ERROR;
      break;
  } 
  return R1_response;
}

unsigned short GetR1Response(void)
{
  unsigned int i = 0;

  //collect the response in a RAM first
  ReadResponse(2);
  
  //find the first response byte in the buffer
  while ( (i < RESPONSE_BUFFER_LENGTH) && ((mmc_response_buffer[i++] >> 7) == 1) );
   
  return InterpretR1(mmc_response_buffer[i-1]);
}

unsigned short GetR2Response(void)
{
  unsigned int i = 0;
  
  //correct the response in a RAM first
  ReadResponse(3);
  
  //find the first response byte in the buffer
  while ( (i < RESPONSE_BUFFER_LENGTH) && ((mmc_response_buffer[i++] >> 7) == 1) );

  /*R2*/
  switch (mmc_response_buffer[i])
  {
    case 0x01:
      R2_response = (unsigned short)R2_CARD_IS_LOCKED;
      break;
    case 0x02:
      R2_response = (unsigned short)R2_WP_ERASE_SKIP;
      break;
    case 0x04:
      R2_response = (unsigned short)R2_ERROR;
      break;
    case 0x08:
      R2_response = (unsigned short)R2_CC_ERROR;
      break;
    case 0x10:
      R2_response = (unsigned short)R2_CARD_ECC_FAILED;
      break;
    case 0x20:
      R2_response = (unsigned short)R2_WP_VIOLATION;
      break;
    case 0x40:
      R2_response = (unsigned short)R2_ERASE_PARAM;
      break;
    case 0x80:
      R2_response = (unsigned short)R2_OUT_OF_RANGE;
      break;
  }
  return (InterpretR1(mmc_response_buffer[i-1]) | R2_response);
}

unsigned short GetR3orR7Response(unsigned int *ret_data)
{
  unsigned int i = 0;
  
  //collect the response in a RAM first
  ReadResponse(6);
  
  //find the first response byte in the buffer
  while ( (i < RESPONSE_BUFFER_LENGTH) && ((mmc_response_buffer[i++] >> 7) == 1) );
  
  *ret_data = (mmc_response_buffer[i] << 24) | (mmc_response_buffer[i+1] << 16) | (mmc_response_buffer[i+2] << 8) | mmc_response_buffer[i+3];
  
  return InterpretR1(mmc_response_buffer[i-1]);
}

unsigned char GetCardCapacityStatus(unsigned long response_data)
{
  return ( (response_data & 0x40000000) >> 30 );
}

unsigned char GetPowerUpStatus(unsigned long response_data)
{
  return ( (response_data & 0x80000000) >> 31 );    
}

unsigned short GetOperationVoltageRange(unsigned long response_data)
{
  return (unsigned short)( (response_data & 0x00FF8000) >> 15);
}

unsigned short ReadCSDRegister(CSDRegister *csdReg)
{
  unsigned int i = 0;
  unsigned short R1_response = 0xFFFF;
  
  //collect the response in a RAM first
  ReadResponse(20);
  
  //find the first response byte in the buffer
  while ( (i < RESPONSE_BUFFER_LENGTH) && ((mmc_response_buffer[i++] >> 7) == 1) );

  //get an R1 response
  R1_response = InterpretR1(mmc_response_buffer[i-1]);
  
  if (R1_response == R1_COMPLETE)
  {
    //CSD_STRUCTURE
    i += 2;
    csdReg->CsdStructure                  = (unsigned char)( (mmc_response_buffer[i] & 0xC0) >> 6 );
    i += 5;
    csdReg->ReadBlockLength               = (unsigned short)( 1 << mmc_response_buffer[i] );
    i += 1;
  
    if csdReg->CsdStructure == 0   /*1.0*/
    {
      unsigned long c_size                = ((mmc_response_buffer[i] & 0x03) << 10)  |
                                             (mmc_response_buffer[i+1] << 2)         |
                                             (mmc_response_buffer[i+2] & 0x03);
      unsigned long c_size_mult           = ((mmc_response_buffer[i+3] & 0x03) << 1) |
                                             (mmc_response_buffer[i+4] & 0x01);
      csdReg->NumBlocks                   = (c_size  + 1) * (1 << (c_size_mult + 2));
    }
    else /*csd_version == 2.0*/
    {
      unsigned long c_size                = ((mmc_response_buffer[i+1] << 16)        |
                                             (mmc_response_buffer[i+2] <<  8)        |
                                              mmc_response_buffer[i+3]);
      csdReg->NumBlocks                   = c_size  + 1; 
    }
    i += 4;
    csdReg->EraseSingleBlockEnable        = (unsigned char)((mmc_response_buffer[i] & 0x40) >> 6);
    csdReg->ErasableSectorSize            = (unsigned short)(((mmc_response_buffer[i] & 0xEF) << 1) | (mmc_response_buffer[i+1] >> 7));
    i += 2;
    csdReg->WriteBlockLength              = (unsigned short)( ((mmc_response_buffer[i] & 0x03) << 2) | ((mmc_response_buffer[i+1] &0xC0) >> 6) );
    i += 2;
    csdReg->TemporaryWriteProtection      = (unsigned char)((mmc_response_buffer[i] & 0x10) >> 4); 
  }
  
  return;
}

unsigned long ReadBlocks(CSDRegister csdReg, unsigned char *address, unsigned long numBytes)
{
  unsigned long numBytesRead = 0;
  unsigned long numBlocks = 1 + numBytes/csdReg.ReadBlockLength;
  
  if (1 == numBlocks)   /*single block read*/
  {
    /*issue CMD_17 (READ_SINGLE_BLOCK)*/
    mmcSendCmd(CMD_17, address, 0xFF);
    
    /*read the corresponding response*/
    ReadResponse(2);
     
    /*interpret the response*/
    if (GetR1Response() != R1_COMPLETE)
    {
      return;
    }
    
    /*read a block of data*/
    ReadBuffer(csdReg.ReadBlockLength); 
  
    /*read a CRC*/
    ReadResponse(2);    
    
    numBytesRead = numBytes;

  }
  else  /*multiple block read*/
  {
    unsigned int b = 0;
      
    /*issue CMD_18 (READ_SINGLE_BLOCK)*/
    mmcSendCmd(CMD_18, address, 0xFF);
    
    /*read the corresponding response*/
    ReadResponse(2);
  
    if (GetR1Response() == R1_COMPLETE)
    {
      numBytesRead = csdReg.ReadBlockLength;
    }
    
    for (b = 0; b < numBlocks; b++)
    {
      /*read a block of data*/
      ReadBuffer(csdReg.ReadBlockLength);
    
      /*read a CRC*/
      ReadResponse(2);    
      
      numBytesRead += csdReg.ReadBlockLength);
    }
  }
  
  return numBytesRead;
}

unsigned long WriteBlocks(CSDRegister csdReg, unsigned 

