//NIXIE COUNTDOWN TIMER FOR NCT818 by GRA & AFCH (fominalec@gmail.com)

//v1.4 15.10.2025
//Added: IR remote OFF button support
//v1.3 15.10.2025
//Added: IR remote support
//v1.2 14.10.2025
//***********************************
//Mode button - Start/Pause
//Mode(LONG) - Set (/return)
//Up button - Increase (while edit mode)
//Down button - Decrease (while edit mode)

#include <SPI.h>
#include <Tone.h>
#include <ClickButton.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

//const byte BlueLedPin = 3;
const byte pinBuzzer = 2;
const byte pinSet = A0;
const byte pinUp = A2;
const byte pinDown = A1;
const byte LEpin = 10;  //pin Latch Enabled data accepted while HI level

#define RHV5222PIN 8
bool HV5222;
uint8_t blinkMask = 0;

String stringToDisplay = "00000000";  // Conten of this string will be displayed on tubes (must be 8 chars length)

//                            0      1      2      3      4      5      6      7      8       9
unsigned int SymbolArray[10] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 };

Tone tone1;

#define PIN 6
#define NUMPIXELS 8
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

//buttons pins declarations
ClickButton setButton(pinSet, LOW, CLICKBTN_PULLUP);
ClickButton upButton(pinUp, LOW, CLICKBTN_PULLUP);
ClickButton downButton(pinDown, LOW, CLICKBTN_PULLUP);
///////////////////

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
ISR(TIMER4_COMPA_vect) {
  sei();
  doIndication();
}

void timerSetup() {
  //timer3 setup for calling doIndication function
  TCCR4A = 0;                                         //control registers reset (WGM21, WGM20)
  TCCR4B = 0;                                         //control registers reset
  TCCR4B = (1 << CS12) | (1 << CS10) | (1 << WGM12);  //prescaler 1024 and CTC mode
  //OCR5A = 31; //2 mS
  TCNT4 = 0;  //reset counter to 0
  //OCR4A = 46; //3mS //More Noise ~-70dB 6khz, 12 khz
  OCR4A = 92;              //6mS //Less Noise ~-80dB 6khz, 12 khz
  TIMSK4 = (1 << OCIE1A);  //TIMER3_COMPA_vect interrupt enable
  sei();
}
#endif

int32_t counter = 0;
int32_t counterStartValue = 0;
bool isRunning = false;
bool editMode = false;
int editPosition = 0;
unsigned long prevTickTime = millis();
int minutes, seconds;
bool LD, UD;
uint8_t digitsArray[6] = { 0, 0, 5, 9, 5, 9 };
//uint8_t digitsArray[8]={1,2,3,4,5,6};
//                        h h:m m:s s
int limit = 9;
unsigned long timePassed = 0;

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
#include <IRremote.h>
int RECV_PIN = 4;
IRrecv irrecv(RECV_PIN);
decode_results IRresults;
// buttons codes for remote controller Sony RM-X151
#define IR_BUTTON_UP_CODE     0x6621
#define IR_BUTTON_DOWN_CODE   0x2621
#define IR_BUTTON_MODE_CODE   0x7121
#define IR_BUTTON_MENU_CODE   0x2821
#define IR_BUTTON_LEFT_CODE   0x5621
#define IR_BUTTON_RIGHT_CODE  0x1621
#define IR_BUTTON_OFF_CODE    0x5821
#define IR_BUTTON_ENTER_CODE  0x1D21


class IRButtonState
{
  public:
    int PAUSE_BETWEEN_PACKETS = 50;
    int PACKETS_QTY_IN_LONG_PRESS = 18;

  private:
    bool Flag = 0;
    byte CNT_packets = 0;
    unsigned long lastPacketTime = 0;
    bool START_TIMER = false;
    int _buttonCode;

  public: IRButtonState::IRButtonState(int buttonCode)
    {
      _buttonCode = buttonCode;
    }

  public: int IRButtonState::checkButtonState(int receivedCode)
    {
      if (((millis() - lastPacketTime) > PAUSE_BETWEEN_PACKETS) && (START_TIMER == true))
      {
        START_TIMER = false;
        if (CNT_packets >= 2) {
          Flag = 0;
          CNT_packets = 0;
          START_TIMER = false;
          return 1;
        }
        else {
          Flag = 0;
          CNT_packets = 0;
          return 0;
        }
      }
      else
      {
        if (receivedCode == _buttonCode) {
          Flag = 1;
        }
        else
        {
          if (!(Flag == 1)) {
            return 0;
          }
          else
          {
            if (!(receivedCode == 0xFFFFFFFF)) {
              return 0;
            }
          }
        }
        CNT_packets++;
        lastPacketTime = millis();
        START_TIMER = true;
        if (CNT_packets >= PACKETS_QTY_IN_LONG_PRESS) {
          Flag = 0;
          CNT_packets = 0;
          START_TIMER = false;
          return -1;
        }
        else {
          return 0;
        }
      }
    }
};

IRButtonState IRModeButton(IR_BUTTON_MODE_CODE);
IRButtonState IRMenuButton(IR_BUTTON_MENU_CODE);
IRButtonState IRUpButton(IR_BUTTON_UP_CODE);
IRButtonState IRDownButton(IR_BUTTON_DOWN_CODE);
IRButtonState IREnterButton(IR_BUTTON_ENTER_CODE);
IRButtonState IRLeftButton(IR_BUTTON_LEFT_CODE);
IRButtonState IRRightButton(IR_BUTTON_RIGHT_CODE);
IRButtonState IROffButton(IR_BUTTON_OFF_CODE);
#endif

int ModeButtonState = 0;
int MenuButtonState = 0;
int UpButtonState = 0;
int DownButtonState = 0;
int EnterButtonState = 0;
int LeftButtonState = 0;
int RightButtonState = 0;
int OffButtonState = 0;
//IR remote control /////////// END ////////////////////////////

bool OffState = false;

/*******************************************************************************************************
Init Programm
*******************************************************************************************************/
void setup() {
  Serial.begin(115200);
  Serial.println(F("Сountdown timer for NCS318 ver. 1.3"));

  LEDsSetup();
  SetLEDsBlue();

  pinMode(LEpin, OUTPUT);
  pinMode(pinSet, INPUT_PULLUP);
  pinMode(pinUp, INPUT_PULLUP);
  pinMode(pinDown, INPUT_PULLUP);
  pinMode(pinBuzzer, OUTPUT);

  // SPI setup
  SPI_Init();
  //stringToDisplay="006000";
  doIndication();
  //pinMode(BlueLedPin, OUTPUT);
  //analogWrite(BlueLedPin, 50);
  tone1.begin(pinBuzzer);
  //tone1.play(1000, 100);

  //Serial.println("Setting up the counter");
  //setCounter();

  EEPROM.get(0, counter);
  counterStartValue = counter;
  Serial.print("counter=");
  Serial.println(counter);
  if ((counter > 359999) || (counter <= 0)) setCounter();

  //buttons objects inits
  setButton.debounceTime = 20;     // Debounce timer in ms
  setButton.multiclickTime = 30;   // Time limit for multi clicks
  setButton.longClickTime = 1000;  // time until "held-down clicks" register

  upButton.debounceTime = 20;     // Debounce timer in ms
  upButton.multiclickTime = 30;   // Time limit for multi clicks
  upButton.longClickTime = 2000;  // time until "held-down clicks" register

  downButton.debounceTime = 20;     // Debounce timer in ms
  downButton.multiclickTime = 30;   // Time limit for multi clicks
  downButton.longClickTime = 2000;  // time until "held-down clicks" register

  //Serial.println("Setting up the timer");

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  timerSetup();
#endif
  //Serial.println("The timer has been set");
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  irrecv.blink13(false);
  irrecv.enableIRIn(); // Start the receiver
#endif
}

/***************************************************************************************************************
MAIN Programm
***************************************************************************************************************/

void loop() 
{
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  IRresults.value = 0;
  if (irrecv.decode(&IRresults)) {
    //Serial.println(IRresults.value, HEX); // debug
    irrecv.resume(); // Receive the next value
  }

  ModeButtonState = IRModeButton.checkButtonState(IRresults.value);
  if (ModeButtonState == 1) Serial.println(F("Mode short"));
  if (ModeButtonState == -1) Serial.println(F("Mode long...."));

  MenuButtonState = IRMenuButton.checkButtonState(IRresults.value);
  if (MenuButtonState == 1) Serial.println(F("Menu short"));
  if (MenuButtonState == -1) Serial.println(F("Menu long...."));

  UpButtonState = IRUpButton.checkButtonState(IRresults.value);
  if (UpButtonState == 1) Serial.println(F("Up short"));
  if (UpButtonState == -1) Serial.println(F("Up long...."));

  DownButtonState = IRDownButton.checkButtonState(IRresults.value);
  if (DownButtonState == 1) Serial.println(F("Down short"));
  if (DownButtonState == -1) Serial.println(F("Down long...."));

  EnterButtonState = IREnterButton.checkButtonState(IRresults.value);
  if (EnterButtonState == 1) Serial.println(F("Enter short"));
  if (EnterButtonState == -1) Serial.println(F("Enter long...."));

  LeftButtonState = IRLeftButton.checkButtonState(IRresults.value);
  if (LeftButtonState == 1) Serial.println(F("Left short"));
  if (LeftButtonState == -1) Serial.println(F("Left long...."));

  RightButtonState = IRRightButton.checkButtonState(IRresults.value);
  if (RightButtonState == 1) Serial.println(F("Right short"));
  if (RightButtonState == -1) Serial.println(F("Right long...."));

  OffButtonState = IROffButton.checkButtonState(IRresults.value);
  if (OffButtonState == 1) Serial.println(F("Off short"));
  if (OffButtonState == -1) Serial.println(F("Off long...."));

#else
  ModeButtonState = 0;
  MenuButtonState = 0;
  UpButtonState = 0;
  DownButtonState = 0;
  EnterButtonState = 0;
  LeftButtonState = 0;
  RightButtonState = 0;
  OffButtonState = 0;
#endif

  if (editMode == true) {
    blinkMask = 1 << editPosition;
  } else blinkMask = 0;

  upButton.Update();
  setButton.Update();
  downButton.Update();

  if (OffButtonState == 1)
  {
    tone1.play(1000, 100);
    OffState = !OffState;
    if (OffState == true) 
    {
      isRunning = false;
      TurnOffLEDs();
    } else 
      {
        TurnOnLEDs();
      }
  }

  if ((OffState == true) && (setButton.clicks == 0)) 
  {
    return;
  } else if (setButton.clicks > 0) 
    {
      tone1.play(1000, 100);
      TurnOnLEDs();
      if (OffState == true) setButton.clicks = 0;
      OffState = false;
    }

  if ((setButton.clicks < 0) || (MenuButtonState == 1) || (ModeButtonState == 1)) { //long click
    //editMode=true;
    tone1.play(1000, 100);
    editMode = !editMode;
    if (editMode == true)
        EnterToEditMode();
      else 
        ExitFromEditMode();
  }

  // if (editPosition > 5) {
  //   ExitFromEditMode();
  // }

  if ((setButton.clicks > 0) || (EnterButtonState == 1)) { //short click
    SetLEDsBlue();
    tone1.play(1000, 100);
    if (editMode == true) {
      editPosition++;
      Serial.print("EditPosition = ");
      Serial.println(editPosition);
    } else {
      if (counter == 0) counter = counterStartValue;
      else isRunning = !isRunning;
      // if ((isRunning == true) && (counter == 0))
      //   counter = counterStartValue;
      Serial.print("Timer is: ");
      Serial.println(isRunning);
    }
  }

  if ((editPosition > 5) || (editPosition < 0)) {
    ExitFromEditMode();
  }

  if (editMode == true) {
    if ((upButton.clicks > 0) || (UpButtonState == 1)) {
      tone1.play(1000, 100);
      if ((editPosition == 2) || (editPosition == 4)) limit = 5;  //tens of minutes or seconds
      else limit = 9;
      if (digitsArray[editPosition] < limit) digitsArray[editPosition]++;
      else digitsArray[editPosition] = 0;
      printDigitsArray();
      updateTubes();
    }

    if ((downButton.clicks > 0) || (DownButtonState == 1)) {
      tone1.play(1000, 100);
      if ((editPosition == 2) || (editPosition == 4)) limit = 5;  //tens of minutes or seconds
      else limit = 9;
      if (digitsArray[editPosition] > 0) digitsArray[editPosition]--;
      else digitsArray[editPosition] = limit;
      printDigitsArray();
      updateTubes();
    }

    if (RightButtonState == 1)
    {
      tone1.play(1000, 100);
      editPosition++;
      Serial.print("EditPosition = ");
      Serial.println(editPosition);
    }

    if (LeftButtonState == 1)
    {
      tone1.play(1000, 100);
      editPosition--;
      Serial.print("EditPosition = ");
      Serial.println(editPosition);
    } 
  }

  if (editMode == false) 
  {
    if ((upButton.clicks > 0) || (UpButtonState == 1))
    {
      tone1.play(1000, 100);
      isRunning = false;
      counter = counterStartValue;
    }

    if ((downButton.clicks > 0) || (DownButtonState == 1))
    {
      tone1.play(1000, 100);
      //isRunning = false;
      counter = 0;
    }
   }

  if (isRunning) doDotBlink();
  timePassed = millis() - prevTickTime;
  if ((timePassed > 1000) && (isRunning)) {
    prevTickTime = millis();
    //Serial.println("tick");
    //Serial.println(counter);
    counter = counter - 1;
    if (counter <= 0) {
      counter = 0;
      isRunning = false;
      Serial.println(F("Stop"));
      tone1.play(400, 1000);
      SetLEDsRed();
    }
    updateTubes();
    //printDigitsArray();
  }
  updateTubes();
}

void setCounter() {
  //counter = (digitsArray[0]*1000UL + digitsArray[1]*100UL + digitsArray[2]*10UL + digitsArray[3])*3600UL + (digitsArray[4]*10UL + digitsArray[5])*60UL + digitsArray[6]*10UL + digitsArray[7];
  counter = (digitsArray[0] * 10UL + digitsArray[1]) * 3600UL + (digitsArray[2] * 10UL + digitsArray[3]) * 60UL + digitsArray[4] * 10UL + digitsArray[5];
}
void printDigitsArray() {
  //Serial.println(digitsArray[0]*10000000UL + digitsArray[1]*1000000UL + digitsArray[2]*100000UL + digitsArray[3]*10000UL + digitsArray[4]*1000UL + digitsArray[5]*100UL + digitsArray[6]*10UL + digitsArray[7]);
  Serial.println(digitsArray[0] * 100000UL + digitsArray[1] * 10000UL + digitsArray[2] * 1000UL + digitsArray[3] * 100UL + digitsArray[4] * 10UL + digitsArray[5]);
}

void setArray() {
  String tmpStr = PreZero4(counter / 3600) + PreZero((counter % 3600) / 60) + PreZero(counter % 60);
  uint32_t tmpInt = tmpStr.toInt();
  // digitsArray[7]=tmpInt%10;
  // tmpInt=tmpInt/10;
  // digitsArray[6]=tmpInt%10;
  // tmpInt=tmpInt/10;
  digitsArray[5] = tmpInt % 10;
  tmpInt = tmpInt / 10;
  digitsArray[4] = tmpInt % 10;
  tmpInt = tmpInt / 10;
  digitsArray[3] = tmpInt % 10;
  tmpInt = tmpInt / 10;
  digitsArray[2] = tmpInt % 10;
  tmpInt = tmpInt / 10;
  digitsArray[1] = tmpInt % 10;
  tmpInt = tmpInt / 10;
  digitsArray[0] = tmpInt;
  Serial.print("digitsArray=");
  printDigitsArray();
}

void updateTubes() {
  if (editMode == true) {
    stringToDisplay = String(digitsArray[0] * 100000UL + digitsArray[1] * 10000UL + digitsArray[2] * 1000UL + digitsArray[3] * 100UL + digitsArray[4] * 10UL + digitsArray[5]);
  } else {
    stringToDisplay = PreZero4(counter / 3600) + PreZero((counter % 3600) / 60) + PreZero(counter % 60);
  }
  //Serial.print("str=");
  //Serial.println(stringToDisplay);
  //doIndication();
}

void doDotBlink() {
  static unsigned long lastTimeBlink = millis();
  static bool dotState = 0;
  if ((millis() - lastTimeBlink) > 500) {
    lastTimeBlink = millis();
    dotState = !dotState;
    if (dotState) {
      //dotPattern = B11000000;
      LD = true;
      UD = true;
    } else {
      //dotPattern = B00000000;
      LD = false;
      UD = false;
    }
    //updateTubes();
  }
}

String PreZero(int digit) {
  //digit = abs(digit);
  if (digit < 10) return String("0") + String(digit);
  else return String(digit);
}

String PreZero4(uint16_t digit) {
  //digit = abs(digit);
  if (digit < 10) return String("000") + String(digit);
  if (digit < 100) return String("00") + String(digit);
  if (digit < 1000) return String("0") + String(digit);
  else return String(digit);
}

#define UpperDotsMask 0x80000000
#define LowerDotsMask 0x40000000
#define TubeON 0xFFFF
#define TubeOFF 0x3C00

void TurnOffAllTubes()
{
  SPI.transfer(0);
  SPI.transfer(0);
  SPI.transfer(0);
  SPI.transfer(0);

  SPI.transfer(0);
  SPI.transfer(0);
  SPI.transfer(0);
  SPI.transfer(0);

  digitalWrite(LEpin, HIGH); //<<-- это правильно H -> L
  digitalWrite(LEpin, LOW); // <<-- это правильно H -> L
}

void doIndication() 
{
#if defined(__AVR_ATmega328P__)
  static unsigned long lastTimeInterval1Started;
  static unsigned long curMicros;
  curMicros = micros();
  if ((curMicros - lastTimeInterval1Started) < 2000 /*fpsLimit*/) return;
  lastTimeInterval1Started = curMicros;
#endif
  if (OffState == true) {TurnOffAllTubes(); return;}

  unsigned long Var32 = 0;

  long digits = stringToDisplay.toInt();
  /**********************************************************
   * Подготавливаем данные по 32бита 3 раза
   * Формат данных [H1][H2}[M1][M2][S1][Y1][Y2]
   *********************************************************/

#if defined(__AVR_ATmega328P__)
  digitalWrite(LEpin, LOW);
#else
  bitClear(PORTB, PB4);
#endif

   //-------- REG 1 -----------------------------------------------
  Var32 = 0;

  Var32 |= (unsigned long)(SymbolArray[digits % 10] & doEditBlink(5) & moveMask()) << 20;  // s2
  digits = digits / 10;

  Var32 |= (unsigned long)(SymbolArray[digits % 10] & doEditBlink(4) & moveMask()) << 10;  //s1
  digits = digits / 10;

  Var32 |= (unsigned long)(SymbolArray[digits % 10] & doEditBlink(3) & moveMask());  //m2
  digits = digits / 10;

  if (LD) Var32 |= LowerDotsMask;
  else Var32 &= ~LowerDotsMask;

  if (UD) Var32 |= UpperDotsMask;
  else Var32 &= ~UpperDotsMask;

  if (HV5222) {
    SPI.transfer(Var32);
    SPI.transfer(Var32 >> 8);
    SPI.transfer(Var32 >> 16);
    SPI.transfer(Var32 >> 24);
  } else {
    SPI.transfer(Var32 >> 24);
    SPI.transfer(Var32 >> 16);
    SPI.transfer(Var32 >> 8);
    SPI.transfer(Var32);
  }
  //-------------------------------------------------------------------------

  //-------- REG 0 -----------------------------------------------
  Var32 = 0;

  Var32 |= (unsigned long)(SymbolArray[digits % 10] & doEditBlink(2) & moveMask()) << 20;  // m1
  digits = digits / 10;

  Var32 |= (unsigned long)(SymbolArray[digits % 10] & doEditBlink(1) & moveMask()) << 10;  //h2
  digits = digits / 10;

  Var32 |= (unsigned long)SymbolArray[digits % 10] & doEditBlink(0) & moveMask();  //h1
  digits = digits / 10;

  if (LD) Var32 |= LowerDotsMask;
  else Var32 &= ~LowerDotsMask;

  if (UD) Var32 |= UpperDotsMask;
  else Var32 &= ~UpperDotsMask;

  if (HV5222) {
    SPI.transfer(Var32);
    SPI.transfer(Var32 >> 8);
    SPI.transfer(Var32 >> 16);
    SPI.transfer(Var32 >> 24);
  } else {
    SPI.transfer(Var32 >> 24);
    SPI.transfer(Var32 >> 16);
    SPI.transfer(Var32 >> 8);
    SPI.transfer(Var32);
  }

#if defined(__AVR_ATmega328P__)
  digitalWrite(LEpin, HIGH);
#else
  bitSet(PORTB, PB4);
#endif
  //-------------------------------------------------------------------------
}

// void doIndication()
// {

//   static unsigned long lastTimeInterval1Started;
//   if ((micros()-lastTimeInterval1Started)<2000) return;
//   lastTimeInterval1Started=micros();

//   unsigned long Var32=0;

//   long digits=stringToDisplay.toInt();

//   /**********************************************************
//    * Подготавливаем данные по 32бита 3 раза
//    * Формат данных [H1][H2}[M1][M2][S1][Y1][Y2]
//    *********************************************************/
//   //-------- REG 2 -----------------------------------------------
//   Var32|=(unsigned long)(SymbolArray[digits%10]&doEditBlink(7)&moveMask())<<10; // Y2
//   digits=digits/10;

//   Var32 |= (unsigned long)SymbolArray[digits%10]&doEditBlink(6)&moveMask();//y1
//   digits=digits/10;

//   if (LD) Var32&=~LowerDotsMask;
//     else  Var32|=LowerDotsMask;

//   if (UD) Var32&=~UpperDotsMask;
//     else  Var32|=UpperDotsMask;


//   digitalWrite(LEpin, LOW);

//   if (HV5222)
//   {
//     SPI.transfer(Var32);
//     SPI.transfer(Var32>>8);
//     SPI.transfer(Var32>>16);
//     SPI.transfer(Var32>>24);
//   } else
//   {
//     SPI.transfer(Var32>>24);
//     SPI.transfer(Var32>>16);
//     SPI.transfer(Var32>>8);
//     SPI.transfer(Var32);
//   }
//  //-------------------------------------------------------------------------

//  //-------- REG 1 -----------------------------------------------
//   Var32=0;

//   Var32|=(unsigned long)(SymbolArray[digits%10]&doEditBlink(5)&moveMask())<<20; // s2
//   digits=digits/10;

//   Var32|=(unsigned long)(SymbolArray[digits%10]&doEditBlink(4)&moveMask())<<10; //s1
//   digits=digits/10;

//   Var32|=(unsigned long) (SymbolArray[digits%10]&doEditBlink(3)&moveMask()); //m2
//   digits=digits/10;

//   if (LD) Var32&=~LowerDotsMask;
//     else  Var32|=LowerDotsMask;

//   if (UD) Var32&=~UpperDotsMask;
//     else  Var32|=UpperDotsMask;

//   if (HV5222)
//   {
//     SPI.transfer(Var32);
//     SPI.transfer(Var32>>8);
//     SPI.transfer(Var32>>16);
//     SPI.transfer(Var32>>24);
//   } else
//   {
//     SPI.transfer(Var32>>24);
//     SPI.transfer(Var32>>16);
//     SPI.transfer(Var32>>8);
//     SPI.transfer(Var32);
//   }
//  //-------------------------------------------------------------------------

//  //-------- REG 0 -----------------------------------------------
//   Var32=0;

//   Var32|=(unsigned long)(SymbolArray[digits%10]&doEditBlink(2)&moveMask())<<20; // m1
//   digits=digits/10;

//   Var32|= (unsigned long)(SymbolArray[digits%10]&doEditBlink(1)&moveMask())<<10; //h2
//   digits=digits/10;

//   Var32|= (unsigned long)SymbolArray[digits%10]&doEditBlink(0)&moveMask(); //h1

//   if (LD) Var32&=~LowerDotsMask;
//     else  Var32|=LowerDotsMask;

//   if (UD) Var32&=~UpperDotsMask;
//     else  Var32|=UpperDotsMask;

//   if (HV5222)
//   {
//     SPI.transfer(Var32);
//     SPI.transfer(Var32>>8);
//     SPI.transfer(Var32>>16);
//     SPI.transfer(Var32>>24);
//   } else
//   {
//     SPI.transfer(Var32>>24);
//     SPI.transfer(Var32>>16);
//     SPI.transfer(Var32>>8);
//     SPI.transfer(Var32);
//   }

//   digitalWrite(LEpin, HIGH);
// //-------------------------------------------------------------------------
// }

word doEditBlink(int pos) {
  //if (!BlinkUp) return 0xFFFF;
  //if (!BlinkDown) return 0xFFFF;
  int lowBit = blinkMask >> pos;
  lowBit = lowBit & B00000001;

  static unsigned long lastTimeEditBlink = millis();
  static bool blinkState = false;
  word mask = 0xFFFF;
  static int tmp = 0;  //blinkMask;
  if ((millis() - lastTimeEditBlink) > 300) {
    lastTimeEditBlink = millis();
    blinkState = !blinkState;
    if (blinkState) tmp = 0;
    else tmp = blinkMask;
  }

  if ((blinkState == true) && (lowBit == 1)) mask = 0x3C00;  //mask=B11111111;
  return mask;
}

// word moveMask() {
// #define tubesQuantity 3
//   static int callCounter = 0;
//   static int tubeCounter = 0;
//   word onoffTubeMask;
//   if (callCounter == tubeCounter) onoffTubeMask = TubeON;
//   else onoffTubeMask = TubeOFF;
//   callCounter++;
//   if (callCounter == tubesQuantity) {
//     callCounter = 0;
//     tubeCounter++;
//     if (tubeCounter == tubesQuantity) tubeCounter = 0;
//   }
//   return onoffTubeMask;
// }

word moveMask()
{

  #define INIT_STATE B11000000
  static uint8_t mask = INIT_STATE;
  static uint8_t callCounter = B10000000;
  uint16_t onoffTubeMask;

  if (callCounter == B000010)
  {
    callCounter = B10000000;
    mask = mask >> 2;
    if (mask == B00000011) mask = INIT_STATE;
  }
 
  if ((mask & callCounter) == 0) onoffTubeMask = TubeOFF;
    else onoffTubeMask = TubeON;
  
  callCounter = callCounter >> 1;
  return onoffTubeMask;
}

void SPI_Init() {
  pinMode(RHV5222PIN, INPUT_PULLUP);
  HV5222 = !digitalRead(RHV5222PIN);
  SPI.begin();  //
  if (HV5222)
    SPI.beginTransaction(SPISettings(2000000, LSBFIRST, SPI_MODE2));
  else SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE2));
}

void LEDsSetup() {
  pixels.begin();  // This initializes the NeoPixel library.
  pixels.setBrightness(50);
}

void SetLEDsBlue() {
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 255));
  }
  pixels.show();  // This sends the updated pixel color to the hardware.
}

void SetLEDsRed() {
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 0, 0));
  }
  pixels.show();  // This sends the updated pixel color to the hardware.
}

void TurnOffLEDs()
{
  pixels.setBrightness(0);
  pixels.show();
}

void TurnOnLEDs()
{
  pixels.setBrightness(50);
  SetLEDsBlue();
  pixels.show();
}

void EnterToEditMode()
{
  setArray();
  Serial.println("Edit Mode ON");
}

void ExitFromEditMode()
{
  editPosition = 0;
  editMode = false;
  setCounter();
  counterStartValue = counter;
  Serial.println("Edit Mode OFF");
  // Serial.print("counter=");
  // Serial.println(counter);
  EEPROM.put(0, counter);
}

