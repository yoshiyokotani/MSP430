#include "io430.h"
#include "mmc_response.h"
#include "crc.h"

unsigned char mmc_response_buffer[RESPONSE_BUFFER_LENGTH] = {0};
unsigned char mmc_read_write_buffer[515];

void mmcInitBuffers(void)
{
  unsigned int i;
  
  for (i = 0; i < RESPONSE_BUFFER_LENGTH; i++)
  {
    mmc_response_buffer[i] = 0xFF;
  }
  
  for (i = 0; i < 515; i++)
  {
    mmc_read_write_buffer[i] = 0xFF;
  }
}
  
unsigned char mmcWriteReadByte(const unsigned char inputByte)
{
  /* write the input byte into the transmission buffer */
  UCB0TXBUF = inputByte;
  while((UCB0IFG & 0x02) == 0);
  /*it automatically clears UCTXIFG*/
  
  /* read a byte from the reception buffer */
  while((UCB0IFG & 0x01) == 0);
  
  return UCB0RXBUF;
  /*it automatically clears UCRXIFG*/
}

void mmcSendCMD(const short cmd, unsigned long data)
{
  unsigned char temp[6] = {0};

  /*command byte*/
  temp[0] = cmd | 0x40;
  
  /*data bytes*/
  temp[1] = (unsigned char)((data & 0xFF000000) >> 24);
  temp[2] = (unsigned char)((data & 0x00FF0000) >> 16);
  temp[3] = (unsigned char)((data & 0x0000FF00) >>  8);
  temp[4] = (unsigned char)( data & 0x000000FF       );

  /*crc byte + end bit*/
  temp[5] = CalcCRC7(temp) | 0x01;

  /*send the above formed command*/
  mmcWriteReadByte(temp[0]);
  mmcWriteReadByte(temp[1]);
  mmcWriteReadByte(temp[2]); 
  mmcWriteReadByte(temp[3]);
  mmcWriteReadByte(temp[4]);
  mmcWriteReadByte(temp[5]); 

  return;
}

void mmcReadResponse(const unsigned int numBytes)
{
  unsigned int i = 0;
  
  while(i < numBytes)
  {
    mmc_response_buffer[i++]= mmcWriteReadByte(0xFF);
  }
}

void mmcReadBuffer(const unsigned int numBytes)
{
  unsigned int i = 0;
  
  while(i < numBytes)
  {
    mmc_read_write_buffer[i++]= mmcWriteReadByte(0xFF);
  }
}

void mmcWriteBuffer(const unsigned int numBytes)
{
  unsigned int i = 0;
  
  while(i < numBytes)
  {
    mmcWriteReadByte(mmc_read_write_buffer[i++]);
  }
}

unsigned short mmcInterpretR1(const unsigned char response)
{
  unsigned short R1_response = 0xFFFF;

  switch(response)
  {
    case 0x00:
      R1_response = (unsigned short)R1_COMPLETE;
      break;
    case 0x01:
      R1_response = (unsigned short)R1_IN_IDLE_STATE;
      break;
    default:
    {
      switch (response & ~(0x01))
      {
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
      break;
    }      
  }

  return R1_response;
}

unsigned short mmcGetR1Response(const int searchLength)
{
  unsigned int i = 0;
  
  //find the first response byte in the buffer
  while ( (i < searchLength) && ((mmc_response_buffer[i++] >> 7) == 1) );
   
  return mmcInterpretR1(mmc_response_buffer[i-1]);
}

unsigned short mmcGetR1bResponse(void)
{
  unsigned int i = 0;
  unsigned short r1_response = 0xFFFF;

  //find the first response byte in the buffer
  while ( (i < RESPONSE_BUFFER_LENGTH) && ((mmc_response_buffer[i++] >> 7) == 1) );
  if ( (mmc_response_buffer[i-1] >> 7) == 0x00 )
  {
    r1_response = mmcInterpretR1(mmc_response_buffer[i-1]);
  }
  
  //search for the tail of the busy signal
  if (mmc_response_buffer[RESPONSE_BUFFER_LENGTH-1] == 0)
  {
    r1_response |= R1_BUSY;
  } 
  
  return r1_response;
}

unsigned short mmcGetR2Response(void)
{
  unsigned int i = 0;
  unsigned short R2_response = 0xFFFF;
  
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
  return (mmcInterpretR1(mmc_response_buffer[i-1]) | R2_response);
}

unsigned char mmcGetDataResponseToken(unsigned char *isBusy)
{
  unsigned int i = 0;
  unsigned char temp, data_response_token = 0xFF;
  
  *isBusy = 0;
  
  /*data response token*/
  do{
    temp = mmc_response_buffer[i++] & 0x1F;
    if ( (temp == 0x05) || (temp == 0x0B) || (temp == 0x0D) )
    {
      break;
    }
  }while(i < RESPONSE_BUFFER_LENGTH);
  if (i < RESPONSE_BUFFER_LENGTH)
  {
    data_response_token = (unsigned short)(temp >> 1);
  }
  
  /*busy signal*/
  while( (i < RESPONSE_BUFFER_LENGTH) && (mmc_response_buffer[i++] == 0) );
  if (mmc_response_buffer[i-1] == 0)
  {
    *isBusy = 1;
  }
  
  return data_response_token;  
}

unsigned char mmcGetStartBlockToken(void)
{
  unsigned char TokenResponse = TOKEN_START_OK;
  
  if (mmc_read_write_buffer[0] & 0x01 == 0x01)
  {
    TokenResponse = TOKEN_START_NOK;
  }
  
  return TokenResponse;  
}

unsigned short mmcGetR3orR7Response(unsigned long *ret_data)
{
  unsigned int i = 0;
 
  //find the first response byte in the buffer
  while ( (i < RESPONSE_BUFFER_LENGTH) && ((mmc_response_buffer[i++] >> 7) == 1) );
  
  //*ret_data = (mmc_response_buffer[i] << 24) | (mmc_response_buffer[i+1] << 16) | (mmc_response_buffer[i+2] << 8) | mmc_response_buffer[i+3];
  *ret_data = mmc_response_buffer[i] << 8;
  *ret_data |= (mmc_response_buffer[i+1] << 8);
  *ret_data |= mmc_response_buffer[i+2];
  *ret_data <<= 8;
  *ret_data |= mmc_response_buffer[i+3];
  
  return mmcInterpretR1(mmc_response_buffer[i-1]);
}

unsigned char mmcGetCardCapacityStatus(unsigned long response_data)
{
  return ( (response_data & 0x40000000) >> 30 );
}

unsigned char mmcGetPowerUpStatus(unsigned long response_data)
{
  return ( (response_data & 0x80000000) >> 31 );    
}

unsigned short mmcGetOperationVoltageRange(unsigned long response_data)
{
  return (unsigned short)( (response_data & 0x00FF8000) >> 15);
}

unsigned short mmcReadCSDRegister(CSDRegister *csdReg)
{
  unsigned int i = 0;
  unsigned short R1_response = 0xFFFF;

  //find the first response byte in the buffer
  while ( (i < RESPONSE_BUFFER_LENGTH) && ((mmc_response_buffer[i++] >> 7) == 1) );

  //get an R1 response
  R1_response = mmcInterpretR1(mmc_response_buffer[i-1]);
  
  if (R1_response == R1_COMPLETE)
  {
    //CSD_STRUCTURE
    i += 2;
    csdReg->CsdStructure                  = (unsigned char)( (mmc_response_buffer[i] & 0xC0) >> 6 );
    i += 5;
    csdReg->ReadBlockLength               = (unsigned short)( 1 << mmc_response_buffer[i] );
    i += 1;
  
    if (csdReg->CsdStructure == 0)   /*1.0*/
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
      unsigned long c_size = (unsigned long)(mmc_response_buffer[i+1]) << 8;
      c_size |= (unsigned long)(mmc_response_buffer[i+2]);
      c_size <<= 8;
      c_size |= (unsigned long)(mmc_response_buffer[i+3]);
      csdReg->NumBlocks                   = (c_size  + 1)*1000; 
    }
    i += 4;
    csdReg->EraseSingleBlockEnable        = (unsigned char)((mmc_response_buffer[i] & 0x40) >> 6);
    csdReg->ErasableBlocks                = (unsigned short)(((mmc_response_buffer[i] & 0x3F) << 1) | (mmc_response_buffer[i+1] >> 7));
    i += 2;
    csdReg->WriteBlockLength              = 1 << ((unsigned short)( ((mmc_response_buffer[i] & 0x03) << 2) | ((mmc_response_buffer[i+1] &0xC0) >> 6) ));
    i += 2;
    csdReg->TemporaryWriteProtection      = (unsigned char)((mmc_response_buffer[i] & 0x10) >> 4); 
  }
  
  return R1_response;
}