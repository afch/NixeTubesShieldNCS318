//driver for NCS318 HW1.x (registers HV5122)
//driver version 1.0
//1 on register's output will turn on a digit 

//v1.3 can work inside interrupt with Mega (2560)
//v1.2 SPI setup moved to driver's file
//v1.1 Mixed up on/off for dots

#include "doIndication318_HW1.x.h"

#define UpperDotsMask 0x80000000
#define LowerDotsMask 0x40000000
#define TubeON 0xFFFF
#define TubeOFF 0x3C00

void SPISetup()
{
  SPI.begin(); //
  SPI.setDataMode (SPI_MODE2); // Mode 3 SPI
  SPI.setClockDivider(SPI_CLOCK_DIV8); // SCK = 16MHz/128= 125kHz
}

void doIndication()
{
  #if defined (__AVR_ATmega328P__)
  static unsigned long lastTimeInterval1Started;
  static unsigned long curMicros;
  curMicros=micros();
  if ((curMicros-lastTimeInterval1Started)<2000 /*fpsLimit*/) return;
  lastTimeInterval1Started=curMicros;
  #endif
    
  unsigned long Var32=0;
  
  long digits=stringToDisplay.toInt();
  /**********************************************************
   * Подготавливаем данные по 32бита 3 раза
   * Формат данных [H1][H2}[M1][M2][S1][Y1][Y2]
   *********************************************************/

  #if defined (__AVR_ATmega328P__) 
  digitalWrite(LEpin, LOW); 
  #else
  bitClear(PORTB, PB4);
  #endif
  //-------- REG 2 ----------------------------------------------- 
  /*Var32|=(unsigned long)(SymbolArray[digits%10]&doEditBlink(7))<<10; // Y2
  digits=digits/10;

  Var32 |= (unsigned long)SymbolArray[digits%10]&doEditBlink(6);//y1
  digits=digits/10;

  if (LD) Var32|=LowerDotsMask;
    else  Var32&=~LowerDotsMask;
  
  if (UD) Var32|=UpperDotsMask;
    else Var32&=~UpperDotsMask;  
    
  SPI.transfer(Var32>>24);
  SPI.transfer(Var32>>16);
  SPI.transfer(Var32>>8);
  SPI.transfer(Var32);
 //-------------------------------------------------------------------------
*/
 //-------- REG 1 ----------------------------------------------- 
  Var32=0;
 
  Var32|=(unsigned long)(SymbolArray[digits%10]&doEditBlink(5)&moveMask())<<20; // s2
  digits=digits/10;

  Var32|=(unsigned long)(SymbolArray[digits%10]&doEditBlink(4)&moveMask())<<10; //s1
  digits=digits/10;

  Var32|=(unsigned long) (SymbolArray[digits%10]&doEditBlink(3)&moveMask()); //m2
  digits=digits/10;

  if (LD) Var32|=LowerDotsMask;
    else  Var32&=~LowerDotsMask;
  
  if (UD) Var32|=UpperDotsMask;
    else Var32&=~UpperDotsMask;  

  SPI.transfer(Var32>>24);
  SPI.transfer(Var32>>16);
  SPI.transfer(Var32>>8);
  SPI.transfer(Var32);
 //-------------------------------------------------------------------------

 //-------- REG 0 ----------------------------------------------- 
  Var32=0;
  
  Var32|=(unsigned long)(SymbolArray[digits%10]&doEditBlink(2)&moveMask())<<20; // m1
  digits=digits/10;

  Var32|= (unsigned long)(SymbolArray[digits%10]&doEditBlink(1)&moveMask())<<10; //h2
  digits=digits/10;

  Var32|= (unsigned long)SymbolArray[digits%10]&doEditBlink(0)&moveMask(); //h1
  digits=digits/10;

  if (LD) Var32|=LowerDotsMask;
    else  Var32&=~LowerDotsMask;
  
  if (UD) Var32|=UpperDotsMask;
    else Var32&=~UpperDotsMask;  
     
  SPI.transfer(Var32>>24);
  SPI.transfer(Var32>>16);
  SPI.transfer(Var32>>8);
  SPI.transfer(Var32);

  #if defined (__AVR_ATmega328P__)
  digitalWrite(LEpin, HIGH);    
  #else
  bitSet(PORTB, PB4);
  #endif
//-------------------------------------------------------------------------
}

word doEditBlink(int pos)
{
  /*
  if (!BlinkUp) return 0;
  if (!BlinkDown) return 0;
  */

  if (!BlinkUp) return 0xFFFF;
  if (!BlinkDown) return 0xFFFF;
  //if (pos==5) return 0xFFFF; //need to be deleted for testing purpose only!
  int lowBit=blinkMask>>pos;
  lowBit=lowBit&B00000001;
  
  static unsigned long lastTimeEditBlink=millis();
  static bool blinkState=false;
  word mask=0xFFFF;
  static int tmp=0;//blinkMask;
  if ((millis()-lastTimeEditBlink)>300) 
  {
    lastTimeEditBlink=millis();
    blinkState=!blinkState;
    if (blinkState) tmp= 0;
      else tmp=blinkMask;
  }
  if (((dotPattern&~tmp)>>6)&1==1) LD=true;//digitalWrite(pinLowerDots, HIGH);
      else LD=false; //digitalWrite(pinLowerDots, LOW);
  if (((dotPattern&~tmp)>>7)&1==1) UD=true; //digitalWrite(pinUpperDots, HIGH);
      else UD=false; //digitalWrite(pinUpperDots, LOW);
      
  if ((blinkState==true) && (lowBit==1)) mask=0x3C00;//mask=B11111111;
  //Serial.print("doeditblinkMask=");
  //Serial.println(mask, BIN);
  return mask;
}

word blankDigit(int pos)
{
  int lowBit = blankMask >> pos;
  lowBit = lowBit & B00000001;
  word mask = 0;
  if (lowBit == 1) mask = 0xFFFF;
  return mask;
}

word moveMask()
{
  #define tubesQuantity 6
  static int callCounter=0;
  static int tubeCounter=0;
  word onoffTubeMask;
  if (callCounter == tubeCounter) onoffTubeMask=TubeON;
    else onoffTubeMask=TubeOFF;
  callCounter++;
  if (callCounter == tubesQuantity)
  {
    callCounter=0;
    tubeCounter++;
    if (tubeCounter == tubesQuantity) tubeCounter = 0;
  }
  return onoffTubeMask;
}
