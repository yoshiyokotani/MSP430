#include "io430.h"
#include "math.h"
#include "mmc.h"
#include "mmc_response.h"
#include "crc.h"

unsigned long mmcReadBlocks(CSDRegister csdReg, unsigned long address, unsigned long numBytes)
{
  unsigned char start_token = 0xFF;
  unsigned short crc16 = 0xFFFF, r1_response = 0xFFFF;
  unsigned int i = 0;
  unsigned long numBytesRead = 0;
  unsigned long numBlocks = (unsigned long)ceil((float)numBytes/csdReg.ReadBlockLength);
  
  if (1 == numBlocks)   /*single block read*/
  {  
    do 
    {
      /*issue CMD_17 (READ_SINGLE_BLOCK)*/
      mmcSendCMD(CMD_17, address);
    
      /*read the corresponding R1 response*/
      mmcReadResponse(2);
      
      r1_response = mmcGetR1Response(2);
      
      i++;
    }while ( (i < 5) && (r1_response != R1_COMPLETE) );
    
    if (r1_response == R1_COMPLETE)
    {        
      int start_index;

      /*read a block of data including CRC16*/
      mmcReadBuffer(1+csdReg.ReadBlockLength+4);   
    
      /*interpret the responses*/
      start_token = mmcGetStartBlockToken(&start_index);
      if (start_index != -1)
      {
        crc16 = CalcCRC16(mmc_read_write_buffer+start_index, csdReg.ReadBlockLength+2);
      }
      
      if ( (start_token == TOKEN_START_OK)  &&    /*if not, then, data error token was received*/
           (crc16 == 0x0000) )                    /*if not, then, crc error was detected*/
      {
        numBytesRead = numBytes;
      }
    }
  }
  else  /*multiple block read*/
  {
    unsigned long b = 0;
      
    do
    {
      /*issue CMD_18 (READ_SINGLE_BLOCK)*/
      mmcSendCMD(CMD_18, address);
    
      /*read the corresponding response*/
      mmcReadResponse(5);
  
      r1_response = mmcGetR1Response(5);
      
      i++;
    }while( (i < 5) && (r1_response != R1_COMPLETE) );
      
    if (r1_response == R1_COMPLETE)
    {  
      int start_index;
      
      for (b = 0; b < numBlocks; b++)
      {
        crc16 = 0xFFFF;

        /*read a block of data including CRC16*/
        mmcReadBuffer(1+csdReg.ReadBlockLength+4);
        
        /*check the start block token and crc*/
        start_token = mmcGetStartBlockToken(&start_index);
        if (start_index != -1)
        {
          crc16 = CalcCRC16(mmc_read_write_buffer+start_index, csdReg.ReadBlockLength+2);
        }
        
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
            }while(r1b_response == R1_BUSY);
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
  unsigned char start_token, data_response_token, temp, isBusy;
  unsigned short crc16;
  unsigned long numBytesWritten = 0;
  unsigned long numBlocks = (unsigned long)ceil((float)numBytes/csdReg.WriteBlockLength);
  
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
    if (mmcGetR1Response(2) == R1_COMPLETE)
    {
      /*send a data block to write*/
      mmcWriteReadByte(start_token);
      mmcWriteBuffer(csdReg.WriteBlockLength);
      mmcWriteReadByte((unsigned char)(crc16 >> 8));
      mmcWriteReadByte((unsigned char)(crc16 & 0x00FF));
      
      /*wait until the data response token is completely transmitted*/
      isBusy = 0;
      do
      {
        mmcReadResponse(RESPONSE_BUFFER_LENGTH);
        temp = mmcGetDataResponseToken(&isBusy);
        if (temp != 0xFF)
        {
          data_response_token = temp;
        }
      }while(isBusy == 1);
      
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
    
    if (mmcGetR1Response(2) == R1_COMPLETE)
    {
      for (b = 0; b < numBlocks; b++)
      {
        /*send a block of data including CRC16*/
        mmcWriteReadByte(start_token);
        mmcWriteBuffer(csdReg.WriteBlockLength);
        mmcWriteReadByte((unsigned char)(crc16 >> 8));
        mmcWriteReadByte((unsigned char)(crc16 & 0xFF));

        /*wait until the data response token is completely transmitted*/
        isBusy = 0;
        do
        {
          mmcReadResponse(RESPONSE_BUFFER_LENGTH);
          temp = mmcGetDataResponseToken(&isBusy);
          if (temp != 0xFF)
          {
            data_response_token = temp;
          }
        }while(isBusy == 1);        
   
        if (data_response_token == 0x02)  /*Data accepted*/
        {
          numBytesWritten += csdReg.WriteBlockLength;
        }
        else  /*Data rejected -- either CRC error or Write error*/
        {
          /*issue a stop transmission token*/
          mmcWriteReadByte(0xFD);
          
          /*wait until the tail of busy signal is found*/
          isBusy = 0;
          do
          {
            mmcReadResponse(RESPONSE_BUFFER_LENGTH);
            temp = mmcGetDataResponseToken(&isBusy);
            if (temp != 0xFF)
            {
              data_response_token = temp;
            }
          }while(isBusy == 1);          
          
          /*break this block transmission loop*/
          break;
        }   
      }
    }
  }
  
  return numBytesWritten;
}

unsigned char mmcEraseBlocks(unsigned long startAddr, unsigned long endAddr)
{
  unsigned short r1_response = 0xFFFF;
  unsigned short r1b_response = 0xFFFF;  
  
  /*issue CMD_32*/
  mmcSendCMD(CMD_32, startAddr);
  mmcReadResponse(2);
  r1_response = mmcGetR1Response(2);
  if (r1_response != R1_COMPLETE)
  {
    return 0;
  }
  
  /*issue CMD_33*/
  while (1)
  {
    mmcSendCMD(CMD_33, endAddr);
    mmcReadResponse(2);
    r1_response = mmcGetR1Response(2);
    if (r1_response == R1_COMPLETE)
    {
      break;
    }
    //set a wait
    mmcReadResponse(RESPONSE_BUFFER_LENGTH);  
  }

  /*issue CMD_38, and wait for R1b and its tail*/
  do
  {             
    mmcSendCMD(CMD_38, 0x00000000);
    mmcReadResponse(RESPONSE_BUFFER_LENGTH);
    
    /*find an R1 response and the following busy signal*/
    r1b_response = mmcGetR1bResponse();     
  }while( (r1b_response == 0xFFFF) || (r1b_response == R1_BUSY) );
  
  return 1;
}

char mmcInitialization(void)
{
  unsigned char init_complete = 0;
  unsigned int counter;
  unsigned short response = 0x0000;
  unsigned long return_value = 0xFFFFFFFF;

  /*1. initialize buffers*/
  mmcInitBuffers();

  /*2. go to an idle state*/
  counter = 0;
  do
  {
    /*2.1. set the chip select line active*/
    MMC_NSS_Low;

    /*2.2. send a reset (CMD_0) signal*/
    mmcSendCMD(CMD_0,0x00000000);
    mmcReadResponse(2);
    response = mmcGetR1Response(2);
    if (response == R1_IN_IDLE_STATE)
    {
      break;
    }

    /*2.3. set the chip select line inactive*/
    MMC_NSS_High;
    
    counter++;
  }while(counter < 16);
  if (response != R1_IN_IDLE_STATE)
  {
    goto error;
  }
  
  /*3. inquire the host supply voltage*/
  counter = 0;
  do
  {
    mmcSendCMD(CMD_8,0x000001AA);
    mmcReadResponse(6);
    response = mmcGetR3orR7Response(&return_value);
    counter++;
  }while( (response != R1_IN_IDLE_STATE) && (counter < 4) );
  if ( (response != R1_IN_IDLE_STATE) || (return_value != 0x01AA) )
  {
    goto error;
  }      
 
  counter = 0;
  do
  {
    /*4. issue a command to activate the initialization process*/
    mmcSendCMD(CMD_55,0x00000000);
    mmcReadResponse(5);
    response = mmcGetR1Response(5);
    mmcSendCMD(ACMD_41,0x40000000);
    mmcReadResponse(5);      
    response = mmcGetR1Response(5);   
  }while( (response != R1_COMPLETE) && (counter < 4) );
  if (response != R1_COMPLETE)
  {
    goto error;
  }
  
  /*5. read the OCR register*/
  mmcSendCMD(CMD_58,0x00000000);
  mmcReadResponse(6);
  response = mmcGetR3orR7Response(&return_value);
  if (mmcGetPowerUpStatus(return_value) == 1)
  {
    init_complete = 1;
  }

error:
  
  return init_complete;
}

void mmcReadWriteBlockTest(void)
{
  unsigned short response = 0xFFFF;
  unsigned int i = 0, j = 0;
  const unsigned long sectorAddr = 0x00000002;
  unsigned long numBytesToWrite = 0;
  CSDRegister csdReg = {0};
  
  /*1. initialize the card*/
  if(mmcInitialization() == 0)
  { 
    goto error;
  }
 
  i = 0;
  do
  {
    /*2. read the CSD register*/
    mmcSendCMD(CMD_9,0x00000001);
    mmcReadResponse(24);
    response = mmcReadCSDRegister(&csdReg);
    i++;
  }while( (i < 5) && (response != R1_COMPLETE) );
  if (response != R1_COMPLETE)
  {
    goto error;
  }
  
  /*3. erase first and second sectors*/
  if (0 == mmcEraseBlocks(sectorAddr, sectorAddr+1))
  {
    goto error;
  }
  
  /*4. check the erased sectors*/
  for (i = 0; i < 2; i++)
  {
    if (csdReg.ReadBlockLength != mmcReadBlocks(csdReg, sectorAddr+i, csdReg.ReadBlockLength))
    {
      goto error;
    }  
    for (j = 0; j < csdReg.ReadBlockLength; j++)
    {
      if (mmc_read_write_buffer[j] != 0xFF)
      {
        goto error;
      }
    }
  }  
    
  /*4. write a block of data into an MMC*/
  for (i = 0; i < 512; i++)
  {
    mmc_read_write_buffer[i] = i;
  }
  numBytesToWrite = 2 * csdReg.WriteBlockLength;
  if ((unsigned long)(ceil(numBytesToWrite/csdReg.WriteBlockLength)) 
      != mmcWriteBlocks(csdReg, sectorAddr, numBytesToWrite))
  {
    goto error;
  }
    
  /*5. clear mmc_read_writer_buffer*/
  for (i = 0; i < 512; i++)
  {
    mmc_read_write_buffer[i] = 0;
  }
    
  /*6. read a block of data from an MMC*/
  for (i = 0; i < 2; i++)
  {
    if (csdReg.ReadBlockLength != mmcReadBlocks(csdReg, sectorAddr+i, csdReg.ReadBlockLength))
    {
      goto error;
    }  
    for (j = 0; j < csdReg.ReadBlockLength; j++)
    {
      if (mmc_read_write_buffer[j] != j)
      {
        goto error;
      }
    }
  } 
  
error:  
  
  return;
}