
#include "io430.h"

/*global pointers to registers*/
#define P0910BASE 0x0280
#define TA0BASE 0x0340

unsigned char *pP10OUT    = (unsigned char*)(P0910BASE + 0x0003);
unsigned char *pP10DIR    = (unsigned char*)(P0910BASE + 0x0005);

unsigned short *pTA0CTL   = (unsigned short*)(TA0BASE         );
unsigned short *pTA0CCTL0 = (unsigned short*)(TA0BASE + 0x0002);
unsigned short *pTA0CCTL1 = (unsigned short*)(TA0BASE + 0x0004);
unsigned short *pTA0CCR0  = (unsigned short*)(TA0BASE + 0x0012);
unsigned short *pTA0CCR1  = (unsigned short*)(TA0BASE + 0x0014);
unsigned short *pTA0EX0   = (unsigned short*)(TA0BASE + 0x0020);  

int main( void )
{ 
  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;

  __enable_interrupt();
  
  //init LEDs
  //1. set the yellow LED on and the green off
  *pP10OUT = (*pP10OUT | 0x40) & ~(0x80);
 
  //2. set the directions as outputs
  *pP10DIR |= 0xC0;
 
  //init the timer TA0
  //1. stop the timer
  *pTA0CTL &= ~0x0030;
  
  //2.timer clock SMCLK and input divider 1/8
  *pTA0CTL |= 0x02C0;
  
  //3. set the extentional divider 1/8
  *pTA0EX0 |= 0x0007;
    
  //4. set the maximum count values
  *pTA0CCR0 = 0xFFFF;
  *pTA0CCR1 = 0x3FFF;

  //5. enable the interrupt for TA0CCRX
  *pTA0CCTL0 |= 0x0010;
  *pTA0CCTL1 |= 0x0010;
  
  //6. start the timer
  *pTA0CTL |= 0x0010;    //up mode

  while(1);
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void TA01ISR(void)
{
  /*set the yellow LED off and the green on*/
  *pP10OUT = (*pP10OUT & ~(0x40)) | 0x80;
    
  *pTA0CCTL0 &= ~0x0001;
}

#pragma vector = TIMER0_A1_VECTOR
__interrupt void TA02ISR(void)
{
  /*set the yellow LED on and the green off*/
  *pP10OUT = (*pP10OUT | 0x40) & ~(0x80);
              
  *pTA0CCTL1 &= ~0x0001; 
}