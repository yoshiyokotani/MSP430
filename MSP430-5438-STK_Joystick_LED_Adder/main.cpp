//Sample test program for MSP430-5438STK
//Test LEDs and joystick's button
//On pressing of each button the state of LEDs is changed

/*
                up (addition)
  left (number) center (=)      right (division)
                down (multiplication)
*/
#include <stdio.h>
#include "io430.h"

/*
  bit 1 in DIR register -> output
  bit 0 in DIR register -> input
*/

typedef enum JSIN
{
  addition,
  division,
  multiplication,
  equal,
  number,
  invalid
}JSIN;

class Timer0
{
private:
  unsigned short *TA0CTL;
  unsigned short *TA0R;
  unsigned short *TA0EX0;
  unsigned short *TA0IV;
  
public:
  Timer0();
  ~Timer0();
}

class LED
{
private:
  unsigned char *P10BASE;
  unsigned char *P10IN;
  unsigned char *P10OUT;
  unsigned char *P10DIR;
  
public:
  LED();
  ~LED();
  setLEDOn();
  setLEDOff();
}

class JoyStick
{
private:
  unsigned char *P1BASE;
  unsigned char *P1IN;
  unsigned char *P1OUT;
  unsigned char *P1DIR;  
  
public:
  JoyStick();
  ~JoyStick();
  JSIN pollingInput(void);
}

class Calculator
{
public:
  unsigned int doCalculationByJoyStick(void);
}

/*port initialization for the joystick*/
JoyStick::JoyStick()
{
  P1BASE = (unsinged char*)(0x0200);
  P1IN   = P1BASE;
  P1OUT  = P1BASE + 0x0002;
  P1DIR  = P1BASE + 0x0004;
  
  /*clear all the output bits for the joystick*/
  *P1OUT &= ~(0x7C);
  
  /*set the direction -- input*/
  *P1DIR &= ~(0x7C);  
}

/*destructor*/
JoyStick::~JoyStick()
{
  /*clear all the input bits for the joystick*/
  *P1IN &= ~(0x7C);
  
  /*set the direction -- output*/
  *P1DIR |= 0x7C;
}
  
/*do a polling to accept an input from the joystick*/
JSIN JoyStick::pollingInput(void)
{
  unsigned char input = 0;
  JSIN interpreted_input = invalid;
  static unsigned char latch = 0;
  
  while(1)
  {
    input = *P1IN & 0x7C;
    if (input && !latch)
    {
      latch = 1;
      switch(input)
      {
        case 0x04: /*DOWN*/
          interpreted_input = multiplication;
          break;
        case 0x08: /*UP*/
          interpreted_input = addition;
          break;
        case 0x10: /*CENTER*/
          interpreted_input = equal;
          break;
        case 0x20: /*RIGHT*/
          interpreted_input = division;
          break;
        case 0x40: /*LEFT*/
          interpreted_input = number;
          break;
        default:
          interpreted_input = invalid;
          latch = 0;
          break;
      }
      break;
    }
    else if (!input && latch)
    {
      latch = 0;
    }
  }  
  return interpreted_input;
}

/*do a simple calulation a (operation) b = (result) via joystick*/
int Calculator::doCalculationByJoyStick(void)
{
  unsigned char operation = 0;
  int number1 = 0;
  int number2 = 0;
  int result = -1;
  JSIN joystick_input = invalid;
  JoyStick js;

  while(1)
  {
    /*get the first number*/
    while (number == (joystick_input = js.pollingInput()))
    {
      number1++;
    }
    printf("number1: %d\n",number1);
    
    /*get the operation type*/
    operation = (unsigned char)joystick_input;
    printf("operation: ");
    switch(operation)
    {
      case addition:
        printf("addition\n");
        break;
      case division:
        printf("division\n");
        break;
      case multiplication:
        printf("multiplication\n");
        break;
      default:
        printf("invalid\n");
        goto error;
    }
    
    /*get the second number*/
    while (number == (joystick_input = js.pollingInput()))
    {
      number2++;
    }
    printf("number2: %d\n",number2);
    
    /*ensure the equal sign is detected now*/
    if (equal != joystick_input)
    {
      goto error;
    }
    
    /*compute the given operation*/
    switch(operation)
    {
      case addition:
        result = number1 + number2;
        break;
      case division:
        result = number1 / number2;
        break;
      case multiplication:
        result = number1 * number2;
        break;
      default:
        goto error;
    }
    
error:
      /*NOP (result = -1)*/
  
  return result;    
}

void PORT_Init(void)
{
  unsigned char *mP01Dir = (unsigned char*)(0x0200 + 0x04);
  unsigned char *mP01Out = (unsigned char*)(0x0200       );  
  unsigned char *mP10Dir = (unsigned char*)(0x0280 + 0x05);
  unsigned char *mP10Out = (unsigned char*)(0x0280 + 0x03);
  unsigned short *TA0CTL = (unsigned short*)(0x340       );
  unsigned short *TA0EX0 = (unsigned short*)(0x340 + 0x20);
  
  /*clear all the joystick inputs off*/
  *mP01Out &= ~(0x7C);
  
  /*set all the buttons except for the center of the joystick as the input*/
  *mP01Dir &= ~(0x7C);
  
  /*set up the control register for the timer TA0*/
  //*TA0CTL &= ~(0x03F0);
  //*TA0CTL |= 0x0004;    /*timer clear*/
  //*TA0CTL |= 0x03C2;    /*INCLK source, ID=1/8, interrupt enable*/
  
  /*set up the extension register for TA0*/
  //*TA0EX0 &= ~(0x0007);
  //*TA0EX0 |= 0x0007;
  
  /*clear LED output bits*/
  *mP10Out &= ~(0xC0);
    
  /*set both the LEDS as output*/
  *mP10Dir |= 0xC0;
  
  /*set the initial condition of LEDs as yellow on & green off*/
  *mP10Out |= 0x40; 
}

void PORT_Init3(void)
{
  unsigned char *mP01Dir = (unsigned char*)(0x0200 + 0x04);
  unsigned char *mP01In  = (unsigned char*)(0x0200 + 0x00);
  unsigned char *mP10Dir = (unsigned char*)(0x0280 + 0x05);
  unsigned char *mP10Out = (unsigned char*)(0x0280 + 0x03);

  /*clear up, down, right and center buttons of the joystick*/
  *mP01In &= ~(0x4C);
  
  /*set only the up and down buttons of the joystick as the input*/
  *mP01Dir &= ~(0x4C);

  /*clear LED output bits*/
  *mP10Out &= ~(0xC0);
    
  /*set both the LEDS as output*/
  *mP10Dir |= 0xC0;
}
 
int main( void )
{
  unsigned char *mJoyStickIn = (unsigned char*)(0x0200);
  unsigned char *nLEDOut = (unsigned char*)(0x0280 + 0x03);
  unsigned char latch = 0;
  //unsigned short *TA0R = (unsigned short*)(0x0340 + 0x10);
  //unsigned short *TA0CTL = (unsigned short*)(0x340       );
  
  char released = 1;	//optional variable indicate is the button pressed or no
  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;
 
  /*PORT_Init();
  while (1)
  {
	//check state of the buttons and variable "Released"
	if (!((P1IN & 0x40) && (P1IN & 0x20) && (P1IN & 0x10) && (P1IN & 0x08) && (P1IN & 0x04)) && released)
	{	
		P10OUT = P10OUT ^ 0xC0;	//change state of the LEDs
		released = 0;		//button is pressed
	}
	if ((P1IN & 0x40) && (P1IN & 0x20) && (P1IN & 0x10) && (P1IN & 0x08) && (P1IN & 0x04))
		released = 1;		//button is released
  }*/
  
  
      PORT_Init2();
      TA0CTL = (TA0CTL | 1<<9) & ~(1<<8);     //TimerA clock source is SMCLK (bit8=0, bit9=1)
      TA0CTL |= 0x00C0;    

 //   TA0CTL = (TA0CTL | 1<<4) & ~(1<<5);     //Mode Control set to UP mode (MC bits = 0b01)
      TA0CTL = (TA0CTL | 1<<5) & ~(1<<4);     //Mode Control set to Continuous mode (MC bits = 0b10)                  
  
      TA0CTL |= 0x0002;
      
//      TA0CCTL0 = TA0CCTL0 | 0x00A0;           //OUTMODE0 = 0b001 (set mode)
//      TA0EX0 |= 0x0007;
      
      //TA0CCR0 = 0x00;                         //turn off timer
      //TA0CCR0 = 0xFFFF;                       //target value
        
  //TA0CTL |= 0x03E2;
  /*TA0CTL_bit.TASSEL1 = 1;
  TA0CTL_bit.TASSEL0 = 1;
  TA0CTL_bit.TAIE = 1;
  TA0CTL_bit.MC1 = 1;
  TA0CTL_bit.MC0 = 1;*/
  while(1)
  {
    /*if ( !(*mJoyStickIn & 0x10) && (latch == 0) )       //joystick bit == 0 -> stick on
    {
      *nLEDOut ^= 0xC0;
      latch = 1;
    }
    else if ( (*mJoyStickIn & 0x10) && (latch == 1) )   //joystick bit == 1 -> stick off
    {
      latch = 0;
    }*/
    
    //*TA0CTL |= 0x0020;
    
    //if (counter() == 1)
    //if (TA0R == 0xFFFF)
    if (TA0CTL_bit.TAIFG == 1)  //timer interrupt
    //if (TA0CTL_bit.TAIFG == 1) // & 0x0001)
    {
      *nLEDOut ^= 0xC0;
//      *TA0CTL &= ~(0x0001);
      TA0CTL_bit.TAIFG = 0;
    }
    else
    {
      TA0CTL_bit.TAIFG = 0;
    }
  }
  
  /*
  PORT_Init3();
  while(1)
  {
    if ( !(*mJoyStickIn & 0x10) && (latch == 0) )       //joystick bit == 0 -> stick on
    {
      *nLEDOut ^= 0xC0;
      latch = 1;
    }
    else if ( (*mJoyStickIn & 0x10) && (latch == 1) )   //joystick bit == 1 -> stick off
    {
      latch = 0;
    }
  }*/  
    
}

