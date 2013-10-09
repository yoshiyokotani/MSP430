#include "io430.h"
#include "mmc.h"
#include "mmc_response.h"
#include "crc.h"

unsigned long mmcReadBlocks(CSDRegister csdReg, unsigned long address, unsigned long numBytes)
{
  unsigned char start_token = 0xFF;
  unsigned short r1_response = 0xFFFF, crc16 = 0xFFFF;
  unsigned long numBytesRead = 0;
  unsigned long numBlocks = 1 + numBytes/csdReg.ReadBlockLength;
  
  if (1 == numBlocks)   /*single block read*/
  {
    /*issue CMD_17 (READ_SINGLE_BLOCK)*/
    mmcSendCMD(CMD_17, address);
    
    /*read the corresponding R1 response*/
    mmcReadResponse(2);
  
    /*read a block of data including CRC16*/
    mmcReadBuffer(1+csdReg.ReadBlockLength+2);   

    /*interpret the responses*/
    r1_response = mmcGetR1Response();
    start_token = mmcGetStartBlockToken();
    crc16 = CalcCRC16(mmc_read_write_buffer, csdReg.ReadBlockLength);
    
    if ( (r1_response == R1_COMPLETE) &&        /*if not, then, R1 error*/
         (start_token == TOKEN_START_OK)  &&    /*if not, then, data error token was received*/
         (crc16 == 0x0000) )                    /*if not, then, crc error was detected*/
    {
      numBytesRead = numBytes;
    }
  }
  else  /*multiple block read*/
  {
    unsigned long b = 0;
      
    /*issue CMD_18 (READ_SINGLE_BLOCK)*/
    mmcSendCMD(CMD_18, address);
    
    /*read the corresponding response token*/
    mmcReadResponse(2);
  
    if (mmcGetR1Response() == R1_COMPLETE)
    {  
      for (b = 0; b < numBlocks; b++)
      {
        /*read a block of data including CRC16*/
        mmcReadBuffer(1+csdReg.ReadBlockLength+2);
        
        /*check the start block token and crc*/
        start_token = mmcGetStartBlockToken();
        crc16 = CalcCRC16(mmc_read_write_buffer, csdReg.ReadBlockLength);
        
        if ( (start_token == TOKEN_START_OK) && (crc16 == 0x0000) )
        {
          numBytesRead += csdReg.ReadBlockLength;
        }
        else
        {
          /*send the stop transmission command*/
          mmcSendCMD(CMD_12, 0x00000000);
          
          /*wait for R1b and its tail*/
          {
            unsigned short r1b_response = 0xFFFF;
            
            do
            {
              mmcReadResponse(RESPONSE_BUFFER_LENGTH);
    
              /*find an R1 response and the following busy signal*/
              r1b_response = mmcGetR1bResponse();     
            }while( (r1b_response == 0xFFFF) || (r1b_response == R1_BUSY) );
          }
          
          /*break the loop*/
          break;
        }
      }
    }
  }
  
  return numBytesRead;
}

unsigned long mmcWriteBlocks(CSDRegister csdReg, unsigned long address, unsigned long numBytes)
{
  unsigned char start_token, data_response_token;
  unsigned short crc16;
  unsigned long numBytesWritten = 0;
  unsigned long numBlocks = 1 + numBytes/csdReg.WriteBlockLength;
  
  if (1 == numBlocks)   /*single block write*/
  {
    /*set ups*/
    start_token = 0xFE;         /*for WRITE_BLOCK*/
    crc16 = CalcCRC16(mmc_read_write_buffer, csdReg.WriteBlockLength);
    
    /*issue CMD_24 (WRITE_BLOCK)*/
    mmcSendCMD(CMD_24, address);
    
    /*read the corresponding R1 response*/
    mmcReadResponse(2);
     
    /*interpret the response, and if it's a right one, then, send a data block*/
    if (mmcGetR1Response() == R1_COMPLETE)
    {
      /*send a data block to write*/
      mmcWriteReadByte(start_token);
      mmcWriteBuffer(csdReg.WriteBlockLength);
      mmcWriteReadByte((unsigned char)(crc16 >> 8));
      mmcWriteReadByte((unsigned char)(crc16 & 0x00FF));
      
      /*wait until the data response token is completely transmitted*/
      do
      {
        mmcReadResponse(RESPONSE_BUFFER_LENGTH);
        data_response_token = mmcGetDataResponseToken();
      }while( (data_response_token == 0xFF) || (data_response_token == R1_BUSY) );
      
      if (data_response_token == 0x02)  /*Data accepted*/
      {
        numBytesWritten = csdReg.WriteBlockLength;
      }
    }
    /*no further action in case of an errorenous R1 response*/
  }
  else  /*multiple block write*/
  {
    unsigned long b = 0;

    start_token = 0xFC;         /*for WRITE_MULTIPLE_BLOCK*/
    crc16 = CalcCRC16(mmc_read_write_buffer, csdReg.WriteBlockLength);
    
    /*issue CMD_25 (WRITE_MULTIPLE_BLOCK)*/
    mmcSendCMD(CMD_25, address);
    
    /*read the corresponding R1 response*/
    mmcReadResponse(2);
    
    if (mmcGetR1Response() == R1_COMPLETE)
    {
      for (b = 0; b < numBlocks; b++)
      {
        /*send a block of data including CRC16*/
        mmcWriteReadByte(start_token);
        mmcWriteBuffer(csdReg.WriteBlockLength);
        mmcWriteReadByte((unsigned char)(crc16 >> 8));
        mmcWriteReadByte((unsigned char)(crc16 & 0xFF));

        /*wait until the data response token is completely transmitted*/
        do
        {
          mmcReadResponse(RESPONSE_BUFFER_LENGTH);
          data_response_token = mmcGetDataResponseToken();
        }while( (data_response_token == 0xFF) || (data_response_token == R1_BUSY) );        
   
        if (data_response_token == 0x02)  /*Data accepted*/
        {
          numBytesWritten = csdReg.WriteBlockLength;
        }
        else  /*Data rejected -- either CRC error or Write error*/
        {
          /*issue a stop transmission token*/
          mmcWriteReadByte(0xFD);
          
          /*wait until the tail of busy signal is found*/
          do
          {
            mmcReadResponse(RESPONSE_BUFFER_LENGTH);
            data_response_token = mmcGetDataResponseToken();
          }while(data_response_token == R1_BUSY);          
          
          /*break this block transmission loop*/
          break;
        }   
      }
    }
  }
  
  return numBytesWritten;
}

char mmcInitialization(void)
{
  unsigned char init_complete = 0;
  unsigned int counter = 0;
  unsigned short response = 0x0000;
  unsigned long return_value = 0xFFFFFFFF;

  /*0. initialize buffers*/
  mmcInitBuffers();
  
  /*1. set the chip select line active and keep it all the way through*/
  MMC_NSS_Low;
  
  counter = 0;
  do
  {
    /*2. send a reset (CMD_0) signal*/
    mmcSendCMD(CMD_0,0x00000000);
    mmcReadResponse(20);
    response = mmcGetR1Response();
  
    /*3. inquire the host supply voltage*/
    mmcSendCMD(CMD_8,0x000001AA);
    mmcReadResponse(8);
    response = mmcGetR3orR7Response(&return_value);
    
    counter++;
  }while( (return_value != 0x1AA) && (counter < 64) );
 
  if (return_value == 0x1AA)
  {
    counter = 0;
    do
    {
      /*4. issue a command to activate the initialization process*/
      mmcSendCMD(CMD_55,0x00000000);
      mmcReadResponse(5);
      response = mmcGetR1Response();
      mmcSendCMD(ACMD_41,0x40000000);
      mmcReadResponse(5);      
      response = mmcGetR1Response();   
    }while( (response == R1_IN_IDLE_STATE) && (counter < 4) );
    
    if (response == R1_COMPLETE)
    {    
      /*5. read the OCR register*/
      mmcSendCMD(CMD_58,0x00000000);
      mmcReadResponse(6);
      response = mmcGetR3orR7Response(&return_value);
      if (mmcGetPowerUpStatus(return_value) == 1)
      {
        init_complete = 1;
      }
    }
  }
    
  return init_complete;
}

void mmcReadWriteBlockTest(void)
{
  unsigned char response = 0xFF;
  unsigned int i = 0;
  unsigned long return_value = 0xFFFFFFFF;
  CSDRegister csdReg = {0};
  
  /*1. initialize the card*/
  if(mmcInitialization() == 0)
  { 
    return;
  }
  else
  {
    /*2. read the OCR register to check the card capacity and operational voltage*/
    {    
      //OCR
      mmcSendCMD(CMD_58,0x00000000);
      mmcReadResponse(6);
      response = mmcGetR3orR7Response(&return_value);     
      if (response == R1_COMPLETE)
      {
        unsigned char ccs = mmcGetCardCapacityStatus(return_value);
        unsigned short voltage_range = mmcGetOperationVoltageRange(return_value);   /*0x01FF: 2.7-3.6V*/
      }
    }
    
    /*3. read the CID register*/
    mmcSendCMD(CMD_10,0x00000000);
    mmcReadResponse(2);
    response = mmcGetR1Response();
  
    /*4. read the CSD register*/
    mmcSendCMD(CMD_9,0x00000000);
    response = mmcReadCSDRegister(&csdReg);
    
    /*5. write a block of data into an MMC*/
    for (i = 0; i < 512; i++)
    {
      mmc_read_write_buffer[i] = i;
    }  
    mmcWriteBlocks(csdReg, 0x00000000, csdReg.WriteBlockLength);
    
    /*6. clear mmc_read_writer_buffer*/
    for (i = 0; i < 512; i++)
    {
      mmc_read_write_buffer[i] = 0;
    }
    
    /*7. read a block of data from an MMC*/
    mmcReadBlocks(csdReg, 0x00000000, csdReg.ReadBlockLength);
  }
  
  return;
}