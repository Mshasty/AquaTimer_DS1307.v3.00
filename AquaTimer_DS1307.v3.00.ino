/********************************************************
* 6-channel timer for Aquarium. Version 3.00 (15.05.19) *
* creator: Owintsowsky Maxim     21 102 / 18 932 (1535) *
* https://vk.com/automation4house  VK group             *
* https://t.me/aquatimer Telegram chat                  *
*********************************************************/

#include <Wire.h>
#include "RTClib.h"
#include <EEPROM.h> 
#include "GyverButton.h"
#include "TimerOne.h"
#include "pitches.h"

// State for LCD i2c mode
//#define LCD-I2C_MODE 
 
#ifndef LCD-I2C_MODE
#include <LiquidCrystal.h>
#else
#include <LiquidCrystal_I2C.h>
#define DS1307_ADDRESS 0x68
#endif

#define VibroPin 15 //Pins for vibro-motor A1
#define TonePin  17 //Pins for pieze A3
#define BackLightPin 10 // Pins for LCD backlight

// State for test mode  16 784 / 17 252 (1231)
/* Уберите две косые в начале следующей строки для теста без RTC */
//#define TEST_MODE 

RTC_DS1307 rtc; // Init the DS1307
DateTime t;

#ifndef LCD-I2C_MODE
LiquidCrystal lcd(8, 9, 4, 5, 6, 7); // for LCD 16*2 with 6*key
#else
LiquidCrystal_I2C lcd(0x27,16,2);
// SDA pin   -> Arduino Analog 4 (A4)
// SCL pin   -> Arduino Analog 5 (A5)
#endif

#define sig8bit int8_t  // readed key type

const uint8_t TVer = 0x30;
const uint8_t RotaryChar[4] = {0x2D, 3, 0x7C, 0x2F}; // Rotary symbol codes
const char dn[8][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
const char mn[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dec"};


boolean BeepOnOff = true; // Switch on/off beeper every hour
boolean KeyToneOn = true; // Switch on/off keys tone

uint16_t  adc_key_val[5] ={92, 230, 380, 600, 720};  // 08, 98, 255, 409, 640
//uint8_t NUM_KEYS = 5; // Number of keys
uint8_t TimersNum = 24; // Number of timers
uint8_t MaxNumChannels = 6; //Max number of channels
uint8_t ChannelsNum = 4; // Number of relay channels                          Screen:      0123456789ABCDEF 
uint8_t ChPin[] = {11, 13, 2, 3, 16, 12}; // Pins for channels (D11, D13, D2, D3, A2, D12) 11 13 2 3 .A2 12
uint16_t FeedTime1 = 625; // Start time vibro
uint16_t FeedTime2 = 1181;
uint8_t FeedDelay = 13; // Time for vibro * 100mS
boolean FeedOK = false; // Feeding held
uint8_t ChOnOff[6]; // Buffer for timers
// My symbols
uint8_t triang[8]  = {0x0,0x8,0xc,0xe,0xc,0x8,0x0}; // >
uint8_t cosaya[8] = {0x0,0x10,0x8,0x4,0x2,0x1,0x0}; 
uint8_t mybell[8]  = {0x4,0xe,0xe,0xe,0x1f,0x0,0x4}; // bell
uint8_t nobell[8]  = {0x5,0xf,0xe,0xe,0x1f,0x8,0x10};
uint8_t check[8] = {0x0,0x1,0x3,0x16,0x1c,0x8,0x0}; // V
uint8_t cross[8] = {0x0,0x1b,0xe,0x4,0xe,0x1b,0x0}; // x
uint8_t smboff[8] = {0x0,0x0,0xe,0xa,0xe,0x0,0x0};  // o
uint8_t setpic = 2; 	// Set-symbol number on table
uint8_t oldsec = 65; 	// Previous second value
int8_t TimeAdj = 0; 	// Time correction value

uint8_t TmrAddr = 16; 	// Timers start address in EEPROM (0xAA, 0x55, TimersNum, ChannelsNum, TimeEatHour, TimeEatMin, EatDelay, 1-RelayUp, HourBeep, KeyTone, TimeAdj, 0, 0, 0, 0, 0)
uint8_t daysofweek[] = {127, 31, 96, 1, 2, 4, 8, 16, 32, 64, 0}; // States in the week
uint8_t WeekStateNum; // Total state options in the week
uint8_t RotaryNum = 0; 
uint8_t RotaryMaxNum = 3;

uint8_t RelayUp = HIGH; // Relay type

uint16_t TimeNow;
boolean BeepNow = false;

boolean Ch1NeedOn = false;
boolean Ch1NeedOff = false;
boolean Ch1OnOff = false;
boolean Ch2NeedOn = false;
boolean Ch2NeedOff = false;
boolean Ch2OnOff = false;
// variables for backlight
uint16_t BLset[2] = {1023, 220}; 	// 255 - day mode, 55 - night mode
boolean BLNeedOn = false;
boolean BLNeedOff = false;
uint8_t BLNightState = 0; 		// 0 - Set day mode, 1 - Set night mode

char LCDbuff0[17];
char LCDbuff1[17];

GButton myButt0(14, HIGH_PULL, NORM_OPEN);
GButton myButt1(14, HIGH_PULL, NORM_OPEN);
GButton myButt2(14, HIGH_PULL, NORM_OPEN);
GButton myButt3(14, HIGH_PULL, NORM_OPEN);
GButton myButt4(14, HIGH_PULL, NORM_OPEN);

#include <GyverTimer.h>
GTimer_ms keypadTimer(10);
GTimer_ms clockTimer(250);
GTimer_ms rotateTimer(280);  

void setup() {
    for (uint8_t ij = 0 ; ij < MaxNumChannels ; ij++) {
        pinMode(ChPin[ij], OUTPUT);
        digitalWrite(ChPin[ij], LOW);
    }
    pinMode(VibroPin, OUTPUT);
    digitalWrite(VibroPin, LOW);
    pinMode(TonePin, OUTPUT);
    digitalWrite(TonePin, LOW);
  
    WeekStateNum = sizeof(daysofweek); 	// Number of states in the week
  
    Wire.begin();
    rtc.begin();
    
    Timer1.initialize(10000);           		// set timer № 1 every 10000 mks (= 10 мs)
    Timer1.attachInterrupt(timerIsr);		// timer start
    
    
#ifdef LCD-I2C_MODE
    lcd.init();
    lcd.begin(16, 2);
    lcd.backlight();
#else
    lcd.begin(16, 2);
#endif
    //analogWrite(BackLightPin, 255); // Backlight On
	Timer1.pwm(BackLightPin, 1023);
    lcd.createChar(1, nobell);
    lcd.createChar(2, triang);
    lcd.createChar(3, cosaya);
    lcd.createChar(4, mybell);
    lcd.createChar(5, check);
    lcd.createChar(6, cross);
    lcd.createChar(7, smboff);
    lcd.home();
    lcd.noCursor();
    ReadWriteEEPROM();
    ShowFeedingTime();
    myDelay(2000);
    //DateTime t = rtc.now(); // Get data from the DS1307
}

void LCDbprint(uint8_t StrQty) { // print buffer to LCD
    if ((StrQty & 1)) { // print upper string
        lcd.setCursor(0, 0); 
        lcd.print(LCDbuff0); 
    }
    if (StrQty & 2) { // print low string
        lcd.setCursor(0, 1); 
        lcd.print(LCDbuff1); 
    }
}

void ShowChannels() {
    snprintf(LCDbuff0, 17, "Out-channels hex"); 
    snprintf(LCDbuff1, 17, "                ");
    LCDbprint(3);
    lcd.setCursor(0, 1);
    for (uint8_t ij = 0 ; ij < MaxNumChannels ; ij++) {
        lcd.print(ChPin[ij], HEX);
        if ((ij+1) < MaxNumChannels) lcd.write(32);
        else if (((ij+1) == MaxNumChannels) && (ChannelsNum < MaxNumChannels)) lcd.write(41);
        if (((ij+1) == ChannelsNum) && (ChannelsNum < MaxNumChannels)) lcd.write(40);
    }
}

void ShowFeedingTime() {
    snprintf(LCDbuff0, 17, "Feed %02d:%02d %02d:%02d", EEPROM.read(2), EEPROM.read(3), EEPROM.read(4), EEPROM.read(5));
    snprintf(LCDbuff1, 17, "\4 duration%2d.%d s", FeedDelay/10, FeedDelay%10);
    LCDbprint(3);
}

String lid1Zero(uint8_t val) {
    if (val<10) return "0" + String(val);
    else return String(val);
}

// Written massive to EEPROM from adderess <addr> and size of <dtlng>
void eeWrite(uint8_t val[], uint16_t addr, uint16_t dtlng) {
    for(uint8_t i=0; i<dtlng; i++) {
        EEPROM.write(i+addr, val[i]);
    }
}

void EEwritedef() { // Written default value to EEPROM for all timers
    // for ver.2.15 & above        0    1            2             3            4             5            6         7        8         9        10       11            12              13 14 15
    // default values for EEPROM: (Ver, ChannelsNum, TimeEatHour1, TimeEatMin1, TimeEatHour2, TimeEatMin2, EatDelay, RelayUp, HourBeep, KeyBeep, TimeAdj, BackLightDay, BackLightNight, 0, 0, 0, Timers defult setting)
    //                      0     1  2  3   4   5   6  7  8  9  10 11   12  13 14 15 |<--     Timer1     -->|<--      Timer2      -->|<--     Timer3     -->|<--    Timer4    -->|
    uint8_t ChOnOffDef[] = {0x2E, 4, 9, 25, 19, 45, 3, 0, 1, 1, 0, 255, 55, 0, 0, 0, 0x1F, 0, 7, 20, 21, 0, 0x7F, 1, 16, 40, 19, 20, 0x60, 0, 9, 30, 21, 0, 0x2A, 2, 7, 55, 21, 55};
    ChOnOffDef[1] = ChannelsNum;
    ChOnOffDef[8] = BeepOnOff;
    ChOnOffDef[9] = KeyToneOn;
    ChOnOffDef[10] = TimeAdj;
    eeWrite(ChOnOffDef, 0, sizeof(ChOnOffDef));
    uint8_t ChOnOffNull[6] = {85, 4, 22, 0, 23, 0};
    // write other timers to EEPROM
    for (uint8_t i = 4; i < TimersNum; i++) eeWrite(ChOnOffNull, TmrAddr + i*6, sizeof(ChOnOffNull));
}

void EEreadTimer(uint8_t NumTmr) { // read desired timer
    for (uint8_t i=0; i<6 ; i++) {
        ChOnOff[i] = EEPROM.read(TmrAddr + 6*NumTmr + i);
    }
}

void ReadWriteEEPROM() { // check the contents EEPROM
    if (EEPROM.read(0) == 0x2E) {
        ChannelsNum = EEPROM.read(1);
        FeedTime1 = EEPROM.read(2)*60 + EEPROM.read(3);
        FeedTime2 = EEPROM.read(4)*60 + EEPROM.read(5);
        FeedDelay = EEPROM.read(6);
        RelayUp = HIGH - EEPROM.read(7); //Inverted relay
        BeepOnOff = EEPROM.read(8);
        KeyToneOn = EEPROM.read(9);
        TimeAdj = EEPROM.read(10);
        if (EEPROM.read(11) > 0) {
            BLset[0] = 4*EEPROM.read(11);
            BLset[1] = 4*EEPROM.read(12);
        }
        snprintf(LCDbuff0, 17, "%02d on/off timers", TimersNum);
        snprintf(LCDbuff1, 17, "for %d/%d channels", ChannelsNum, MaxNumChannels);
        LCDbprint(3);
        if (BeepOnOff) {
            tone(TonePin, NOTE_A6, 200);
            myDelay(220);
            tone(TonePin, NOTE_E7, 100);
        }
        myDelay(1780);
        snprintf(LCDbuff0, 17, "Version %2d.%02d by", TVer/16, TVer%16);
        snprintf(LCDbuff1, 17, "  vk.cc/6Z2GQ7  "); // link to https://vk.cc/6Z2GQ7
        LCDbprint(3);
        myDelay(1500);
    } else {
        EEwritedef();
        snprintf(LCDbuff0, 17, "Written time for");
        snprintf(LCDbuff1, 17, "%d channel EEPROM", ChannelsNum);
        LCDbprint(3);
        myDelay(2500);
        ShowChannels();
        tone(TonePin, NOTE_E7, 200);
        myDelay(220);
        tone(TonePin, NOTE_A6, 100);
        myDelay(2000);
    }
}

void HourBeep(uint16_t CurTime) { // makes a beep each hour
    if (BeepOnOff) {
        if (CurTime % 60 == 0) {
            if (!BeepNow) { // if not peak yet
                //int ToneBoy[] = {NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6, NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6};
                //int ToneBoy[] = {NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5, NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6};
                uint16_t ToneBoy[] = {NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6, NOTE_C7, NOTE_CS7, NOTE_D7, NOTE_DS7, NOTE_E7, NOTE_F7, NOTE_FS7, NOTE_G7};
                CurTime /= 60;
                CurTime %= 12;
                tone(TonePin, ToneBoy[CurTime], 31);
                BeepNow = true;
                myDelay(35);
                digitalWrite(TonePin, LOW);
            }
        } else BeepNow = false;
    }
}

void timerIsr() {   // timer interrapt
  	int analog = analogRead(0);
  	myButt4.tick(analog > adc_key_val[3] && analog < adc_key_val[4]); // Menu 641 - 663
  	myButt3.tick(analog > adc_key_val[2] && analog < adc_key_val[3]); // Left 409 - 427
  	myButt2.tick(analog > adc_key_val[1] && analog < adc_key_val[2]); // Down 255 - 287
  	myButt1.tick(analog > adc_key_val[0] && analog < adc_key_val[1]); // Up   99 - 127
  	myButt0.tick(analog >= 0 && analog < adc_key_val[0]);                // Right 0 - 93 
}

void KeyTone() {
  	if (KeyToneOn) tone(TonePin, NOTE_DS6, 15);
}

void ShowTimer(uint8_t i) {
  	lcd.setCursor(0, 0);
  	lcd.print(lid1Zero(i+1));
  	EEreadTimer(i);
  	TimerDisp();
  	lcd.setCursor(1, 0);
  	lcd.blink();
}

String TimerOnOffDisp(uint8_t ChState) {
  	uint8_t kk = 1;
  	String MyOnOffDisp = "       ";
  	if (Ch1NeedOn) {
    		if (ChState & 1) Ch1NeedOn = false;
    		else ChState |= 1;
  	}
  	if (Ch1NeedOff) {
    		if (ChState & 1) ChState &= 14;
    		else Ch1NeedOff = false;
  	}
  	BLNightState = !(ChState & 1);
  	if (BLNeedOn) {
    		if (ChState & 1) BLNeedOn = false;
    		else BLNightState = 0;
  	}
  	if (BLNeedOff) {
    		if (ChState & 1) BLNightState = 1;
    		else BLNeedOff = false;
  	}
  	if (Ch2NeedOn) {
  		  if (ChState & 2) Ch2NeedOn = false;
  		  else ChState |= 2;
  	}
  	if (Ch2NeedOff) {
    		if (ChState & 2) ChState &= 13;
        else Ch2NeedOff = false;
  	}
  	for(uint8_t i=0;i<MaxNumChannels;i++) {
    		if (i < ChannelsNum) { // Only working channels
      			if (kk & ChState) {
        				if (i==1) { // Pump channel
        				    //lcd.write(RotaryChar[RotaryNum]);
          					MyOnOffDisp[i] = RotaryChar[RotaryNum];
        				    RotaryNum++;
        				    if (RotaryNum > RotaryMaxNum) RotaryNum = 0;
        				} else {
        				    //lcd.write(223); // Zaboy
          					MyOnOffDisp[i] = 223;
        				}
        				digitalWrite(ChPin[i], RelayUp);
        				if (i==0) {
        				    Ch1OnOff = true;
        				}
        				if (i==1) {
        				    Ch2OnOff = true;
        				}
      			} else {
        				//lcd.write(161);
        				MyOnOffDisp[i] = 161;
        				digitalWrite(ChPin[i], HIGH-RelayUp);
        				if (i==0) {
        				    Ch1OnOff = false;
        				}
        				if (i==1) {
        				    Ch2OnOff = false;
        				}
      			}
    		} else { // not working channels
    		    //lcd.write(6); // Cross
    			MyOnOffDisp[i] = 6;
    		}
    		kk <<= 1;
  	}
  	//for(uint8_t i=MaxNumChannels;i<6;i++) MyOnOffDisp += " "; // for five and less channel configuration
  	if (BeepOnOff) MyOnOffDisp[6] = 4; // Bell
  	else MyOnOffDisp[6] = 1; // noBell
  	//analogWrite(BackLightPin, BLset[BLNightState]);
	Timer1.setPwmDuty(BackLightPin, BLset[BLNightState]);
  	return MyOnOffDisp;
}

uint8_t StateChannels(uint16_t CurTime, uint8_t MyDayOfWeek) {
  	uint8_t ChState = 0;
  	uint8_t bitDay = 1;
  	if (MyDayOfWeek == 0) MyDayOfWeek = 7;
  	if (MyDayOfWeek > 1) bitDay <<= (MyDayOfWeek-1);
  	for (uint8_t i=0 ; i<TimersNum ; i++) {
    		EEreadTimer(i);
    		if (ChOnOff[0] & bitDay) {
      			uint16_t TimeOn = ChOnOff[2]*60+ChOnOff[3];
      			uint16_t TimeOff = ChOnOff[4]*60+ChOnOff[5];
      			if (TimeOff > TimeOn) {
        				if((CurTime >= TimeOn) && (CurTime < TimeOff)) ChState |= (1 << ChOnOff[1]);
     				} else {
        				if((CurTime >= TimeOn) || (CurTime < TimeOff)) ChState |= (1 << ChOnOff[1]);
      			}
    		}
  	} 
  	return ChState;
}

#ifndef TEST_MODE
//void TimeToLCD(uint8_t DoW, uint8_t myDay, uint8_t myMonth, uint16_t myYear, uint8_t myHour, uint8_t myMin, uint8_t PosCur) {
void TimeToLCD(uint8_t *DoW, uint8_t *myDay, uint8_t *myMonth, uint16_t *myYear, uint8_t *myHour, uint8_t *myMin, uint8_t PosCur) {
    snprintf(LCDbuff1, 17, " %02d.%02d.%02d %02d:%02d ", *myDay, *myMonth, *myYear, *myHour, *myMin); // [ 16.09.17 16:02 ]
    LCDbprint(2);
    lcd.setCursor(PosCur, 1);
    //myDelay(300);
}
#endif

void TimerDisp() {
    lcd.setCursor(2, 0);
    uint8_t kk = 1;
    for(uint8_t j=0;j<7;j++) {
        lcd.write(32);
        if (kk & ChOnOff[0]) lcd.print(j+1);
        else lcd.write(7); // smboff
        kk <<= 1;
    }
    snprintf(LCDbuff1, 17, "Ch%d: %02d:%02d\375%02d:%02d", ChOnOff[1]+1, ChOnOff[2], ChOnOff[3], ChOnOff[4], ChOnOff[5]);
    LCDbprint(2);
}

void SetOneTimer(uint8_t CurTimer) { // Setting current timer parametrs
    uint8_t CurWeekMode;
    uint8_t CurPos = 0;
    uint8_t CurTab[6] = {0, 2, 6, 9, 12, 15};
    for (CurWeekMode=0; CurWeekMode < WeekStateNum; CurWeekMode++) {
        if (ChOnOff[0] == daysofweek[CurWeekMode]) break;
    }
    lcd.blink();
    lcd.setCursor(2, 0);
    lcd.write(setpic);
    lcd.setCursor(2, 0);
    myDelay(450);
    unsigned long TimeStart=millis();
    for(uint8_t MenuExit = 0; MenuExit < 1 ; ) {
        //Read_Key();
        if (myButt0.isClick()) { // Right
            TimeStart=millis();
            KeyTone();
            if (CurPos > 4) CurPos = 0;
            else CurPos++;
            if (CurPos > 0) {
                lcd.setCursor(2, 0);
                lcd.write(32);
                lcd.setCursor(CurTab[CurPos], 1);
            }
            else {
                lcd.setCursor(2, 0);
                lcd.write(setpic);
                lcd.setCursor(2, 0);
            }
        } 
        if (myButt1.isClick()) { //Up
            TimeStart=millis();
            KeyTone();
            if (CurPos > 0) {
                if (CurPos > 1) {
                    if (ChOnOff[CurPos]<(23+(CurPos%2)*32)) { // 23 or 55
                        ChOnOff[CurPos]+=((CurPos%2)*4+1);      // 1 or 5
                    } else {
                        ChOnOff[CurPos]=0;
                    }
                } else {
                    if (ChOnOff[1] < (ChannelsNum-1)) ChOnOff[1]++;
                    else ChOnOff[1]=0;
                }
                TimerDisp();
                lcd.setCursor(CurTab[CurPos], 1);
            } else {
                if (CurWeekMode < (WeekStateNum-1)) CurWeekMode++;
                else CurWeekMode = 0;
                ChOnOff[0] = daysofweek[CurWeekMode];
                TimerDisp();
                lcd.setCursor(2, 0);
                lcd.write(setpic);
                lcd.setCursor(2, 0);
            }
        } 
        if (myButt2.isClick()) { // Down
            TimeStart=millis();
            KeyTone();
            if (CurPos > 0) {
                if (CurPos > 1) {
                    if (ChOnOff[CurPos]>0) { // 23 or 55
                        ChOnOff[CurPos]-=((CurPos%2)*4+1);      // 1 or 5
                    } else {
                        ChOnOff[CurPos]=(23+(CurPos%2)*32);
                    }
                } else {
                    if (ChOnOff[1] > 0) ChOnOff[1]--;
                    else ChOnOff[1]=ChannelsNum-1;
                }
                TimerDisp();
                lcd.setCursor(CurTab[CurPos], 1);
            } else {
                if (CurWeekMode > 0) CurWeekMode--;
                else CurWeekMode = (WeekStateNum-1);
                ChOnOff[0] = daysofweek[CurWeekMode];
                TimerDisp();
                lcd.setCursor(2, 0);
                lcd.write(setpic);
                lcd.setCursor(2, 0);
            }
        } 
        if (myButt3.isClick()) { // Left
            TimeStart=millis();
            KeyTone();
            if (CurPos > 0) CurPos--;
            else CurPos = 5;
            if (CurPos > 0) {
                lcd.setCursor(2, 0);
                lcd.write(32);
                lcd.setCursor(CurTab[CurPos], 1);
            }
            else {
                lcd.setCursor(2, 0);
                lcd.write(setpic);
                lcd.setCursor(2, 0);
            }
        } if (myButt4.isClick()) { // Menu
            KeyTone();
            if (((60*ChOnOff[4]+ChOnOff[5]) < (60*ChOnOff[2]+ChOnOff[3])) && (ChOnOff[0] > 0)) {
                ChOnOff[0] = 127;
                snprintf(LCDbuff0, 17, "Timer night set!");
                snprintf(LCDbuff1, 17, "Full week select");
                LCDbprint(3);
                myDelay(670);
            }
            eeWrite(ChOnOff, TmrAddr + CurTimer*sizeof(ChOnOff), sizeof(ChOnOff));
            MenuExit = 1;
        } // End key read
        if((millis()-TimeStart) > 60000) MenuExit = 1;
    }    
}

void SetTimers() { // Select Timer from Menu
    uint8_t CurTimer = 0;
    ShowTimer(CurTimer);
    myDelay(450);
    unsigned long TimeStart=millis();
    for(uint8_t MenuExit = 0; MenuExit < 1 ; ) {
      //Read_Key();
      if (myButt0.isClick()) { // Right
          KeyTone();
          SetOneTimer(CurTimer);
          ShowTimer(CurTimer);
          myDelay(500);
      } 
    	if (myButt1.isClick()) { //Up
          KeyTone();
          if (CurTimer < (TimersNum-1)) CurTimer++;
          else CurTimer=0;
      		TimeStart=millis();
          ShowTimer(CurTimer);
      } 
    	if (myButt2.isClick()) { // Down
          KeyTone();
          if (CurTimer > 0) CurTimer--;
          else CurTimer = TimersNum - 1;
      		TimeStart=millis();
          ShowTimer(CurTimer);
      } 
    	if (myButt3.isClick()) { // Left
          KeyTone();
          MenuExit = 1;
      } 
    	if (myButt4.isClick()) { // Menu
          KeyTone();
          MenuExit = 1;
      } // End key read
      //myDelay(33);
      if((millis()-TimeStart) > 30000) MenuExit = 1;
    }    
    lcd.noBlink();
}

void TimeSetup() {
#ifndef TEST_MODE
    uint8_t CurTab[5] = {2, 5, 8, 11, 14};
    uint8_t CurPos = 0;
    DateTime tt = rtc.now();
    uint8_t SetDoW = tt.dayOfTheWeek()+1;
    uint8_t SetDay = tt.day();
    uint8_t SetMonth = tt.month();
    uint16_t SetYear = tt.year() - 2000;
    uint8_t SetHour = tt.hour();
    uint8_t SetMin = tt.minute();
    lcd.setCursor(0, 0);
    lcd.print("Set Date & Time:");
    TimeToLCD(&SetDoW, &SetDay, &SetMonth, &SetYear, &SetHour, &SetMin, CurTab[CurPos]);
    //myDelay(1000);
    boolean changeDisp = false;
    lcd.blink();
    unsigned long TimeStart=millis();
    for(uint8_t MenuExit = 0; MenuExit < 1 ; ) {
        //Read_Key();
        if (myButt0.isClick()) {
            if (CurPos < 4) CurPos++; 
            changeDisp = true;
        }
        if (myButt1.isClick() || myButt1.isStep()) { // Up
            switch( CurPos ) {
                case 5:
                    if (SetDoW < 7) SetDoW++;
                    else SetDoW = 1;
                    break;
                case 0:
                    if (SetDay < 31) SetDay++;
                    else SetDay = 1;
                    break;
                case 1:
                    if (SetMonth < 12) SetMonth++;
                    else SetMonth = 1;
                    break;
                case 2:
                    if (SetYear < 99) SetYear++;
                    else SetYear = 0;
                    break;
                case 3:
                    if (SetHour < 23) SetHour++;
                    else SetHour = 0;
                    break;
                case 4:
                    if (SetMin < 59) SetMin++;
                    else {
                        SetMin = 0;
                        if (SetHour < 23) SetHour++;
                        else SetHour = 0;
                    }
                    break;
            }
            changeDisp = true;
        }
        if (myButt2.isClick() || myButt2.isStep()) { // Down
            switch( CurPos ) {
                case 5:
                    if (SetDoW > 1) SetDoW--;
                    else SetDoW = 7;
                    break;
                case 0:
                    if (SetDay > 1) SetDay--;
                    else SetDay = 31;
                    break;
                case 1:
                    if (SetMonth > 1) SetMonth--;
                    else SetMonth = 12;
                    break;
                case 2:
                    if (SetYear > 0) SetYear--;
                    else SetYear = 99;
                    break;
                case 3:
                    if (SetHour > 0) SetHour--;
                    else SetHour = 23;
                    break;
                case 4:
                    if (SetMin > 0) {
                        SetMin--;
                    } else {
                        SetMin = 59;
                        if (SetHour > 0) SetHour--;
                        else SetHour = 23;
                    }
                    break;
            }
            changeDisp = true;
        }
        if (myButt3.isClick()) { // Left
            if (CurPos > 0) CurPos--; 
            changeDisp = true;
        }
        if (myButt4.isClick()) { // Menu
            //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            KeyTone();
            rtc.adjust(DateTime((2000 + SetYear), SetMonth, SetDay, SetHour, SetMin, 0));
            MenuExit = 1;
        }
        if (changeDisp) {
            TimeStart=millis();
        	  KeyTone();
            TimeToLCD(&SetDoW, &SetDay, &SetMonth, &SetYear, &SetHour, &SetMin, CurTab[CurPos]);
            changeDisp = false;
        }
        //myDelay(10);
        if((millis()-TimeStart) > 130000) MenuExit = 1;
    }
#endif
    lcd.noBlink();
}

void FeedDisp(uint8_t Pos) { // Displaying time and duration of feeding
    uint8_t CurTab[5] = {1, 4, 7, 10, 15};
    snprintf(LCDbuff0, 17, "Feed1 Feed2 \256Dur");
    snprintf(LCDbuff1, 17, "%02d:%02d %02d:%02d %2d.%d", EEPROM.read(2), EEPROM.read(3), EEPROM.read(4), EEPROM.read(5), FeedDelay/10, FeedDelay%10);
    LCDbprint(3);
    lcd.setCursor(CurTab[Pos], 1);
}

void FeedMenu() { // Setting the time and duration of feeding
    lcd.blink();
    uint8_t CurPos = 0;
    uint8_t FeedHour1 = EEPROM.read(2);
    uint8_t FeedMin1 = EEPROM.read(3);
    uint8_t FeedHour2 = EEPROM.read(4);
    uint8_t FeedMin2 = EEPROM.read(5);
    FeedDisp(CurPos);
    myDelay(500);
    boolean changeDisp = false;
    unsigned long TimeStart=millis();
    for(uint8_t MenuExit = 0; MenuExit < 1 ; ) {
        //Read_Key();
        if (myButt0.isClick()) { // Right
            if (CurPos > 3) CurPos = 0;
            else CurPos++;
    		    changeDisp = true;
        } 
    	  if (myButt1.isClick()) { //Up
            TimeStart=millis();
        		changeDisp = true;
        		switch( CurPos ) {
                case 0:
                  if (FeedHour1 < 23) FeedHour1++;
                  else FeedHour1 = 0;
                  break;
                case 1:
                  if (FeedMin1 < 59) FeedMin1++;
                  else FeedMin1 = 0;
                  break;
                case 2:
                  if (FeedHour2 < 24) FeedHour2++;
                  else FeedHour2 = 0;
                  break;
                case 3:
                  if (FeedMin2 < 59) FeedMin2++;
                  else FeedMin2 = 0;
                  break;
                case 4:
                  if (FeedDelay < 120) FeedDelay++;
                  else FeedDelay = 0;
                  break;
            }
        } 
      	if (myButt2.isClick()) { // Down
            TimeStart=millis();
        		changeDisp = true;
        		switch( CurPos ) {
                case 0:
                    if (FeedHour1 > 0) FeedHour1--;
                    else FeedHour1 = 23;
                    break;
                case 1:
                    if (FeedMin1 > 0) FeedMin1--;
                    else FeedMin1 = 59;
                    break;
                case 2:
                    if (FeedHour2 > 0) FeedHour2--;
                    else FeedHour2 = 24;
                    break;
                case 3:
                    if (FeedMin2 > 0) FeedMin2--;
                    else FeedMin2 = 59;
                    break;
                case 4:
                    if (FeedDelay > 0) FeedDelay--;
                    else FeedDelay = 120;
                    break;
            }
        } 
    	  if (myButt3.isClick()) { // Left
            if (CurPos < 1) CurPos = 4;
            else CurPos--;
    	    	changeDisp = true;
        } 
      	if (myButt4.isClick()) { // Menu
            KeyTone();
        		MenuExit = 1;
        } // End key read
      	if (changeDisp) {
        		EEPROM.write(2, FeedHour1);
        		EEPROM.write(3, FeedMin1);
        		EEPROM.write(4, FeedHour2);
        		EEPROM.write(5, FeedMin2);
        		EEPROM.write(6, FeedDelay);
        		FeedDisp(CurPos);
        		KeyTone();
        		changeDisp = false;
        }
        if((millis()-TimeStart) > 30000) MenuExit = 1;
    } 
    FeedTime1 = FeedHour1*60 + FeedMin1;
    FeedTime2 = FeedHour2*60 + FeedMin2;
    lcd.noBlink();
}

void SubMenuDisp(uint8_t CursPos, uint8_t MenuPos) { // Display system menu items
    char MenuItem[2][4][17] = {"Positive relay  ", "Hour Beep Off   ", "Keys Tone Off   ", " Day            ", "Inverted relay  ", "Hour Beep On    ", "Keys Tone On    ", " Ngt            "};
    snprintf(LCDbuff0, 17, MenuItem[0][MenuPos]);
    snprintf(LCDbuff1, 17, MenuItem[1][MenuPos]);
    LCDbprint(3);
    switch( MenuPos ) {
        case 0: // Relay type
            lcd.setCursor(15, HIGH-RelayUp);
            break;
        case 1: // Hour beep
            lcd.setCursor(15, BeepOnOff);
            break;
        case 2: // Keys tone
            lcd.setCursor(15, KeyToneOn);
            break;
    }
    lcd.write(5);
    lcd.setCursor(15, CursPos);
    lcd.blink();
}

void SubChangeMenu(uint8_t SysMenuNum) { // System submenu
    uint8_t LastItem = 0;
    SubMenuDisp(LastItem, SysMenuNum);
    myDelay(500);
    unsigned long TimeStart=millis();
    for(uint8_t MenuExit = 0; MenuExit < 1 ; ) {
        //Read_Key();
        if (myButt0.isClick()) { // Right
            EEPROM.write(7+SysMenuNum, LastItem);
            myDelay(30);
            RelayUp=HIGH-EEPROM.read(7);
            BeepOnOff=EEPROM.read(8);
            KeyToneOn=EEPROM.read(9);
            SubMenuDisp(LastItem, SysMenuNum);
            myDelay(700);
            MenuExit = 1;
        } 
      	if (myButt1.isClick()) { //Up
            LastItem = 0;
        		TimeStart=millis();
            SubMenuDisp(LastItem, SysMenuNum);
        } 
      	if (myButt2.isClick()) { // Down
            LastItem = 1;
        		TimeStart=millis();
            SubMenuDisp(LastItem, SysMenuNum);
        } 
      	if (myButt3.isClick()) { // Left
            MenuExit = 1;
        } 
      	if (myButt4.isClick()) { // Menu
                MenuExit = 1;
        } // End key read
        if((millis()-TimeStart) > 20000) MenuExit = 1;
    }
    lcd.noBlink();
}

void DispNumOfChannels() {
    lcd.setCursor(14, 1);
    lcd.print(ChannelsNum);
}

void MenuChannelsNum() {
    snprintf(LCDbuff0, 17, " Set the number ");
    snprintf(LCDbuff1, 17, " of channels:   ");
    LCDbprint(3);
    DispNumOfChannels();
    boolean changeDisp = false;
    unsigned long TimeStart=millis();
    for(uint8_t MenuExit = 0; MenuExit < 1 ; ) {
        //Read_Key();
        if (myButt0.isClick()) { // Right
            EEPROM.write(3, ChannelsNum);
    	    	KeyTone();
            MenuExit = 1;
        } 
      	if (myButt1.isClick()) { //Up
            if (ChannelsNum < MaxNumChannels) ChannelsNum++;
            else ChannelsNum = 2;
        		changeDisp = true;
        } 
      	if (myButt2.isClick()) { // Down
            if (ChannelsNum > 2) ChannelsNum--;
            else ChannelsNum = MaxNumChannels;
    		    changeDisp = true;
        } 
      	if (myButt3.isClick()) { // Left
            ChannelsNum = EEPROM.read(1);
        		KeyTone();
            MenuExit = 1;
        } 
      	if (myButt4.isClick()) { // Menu
            EEPROM.write(1, ChannelsNum);
        		KeyTone();
                MenuExit = 1;
        } // End key read
      	if (changeDisp) {
        		KeyTone();
        		DispNumOfChannels();
        		changeDisp = false;
        		TimeStart=millis();
      	}
        if((millis()-TimeStart) > 15000) {
            ChannelsNum = EEPROM.read(1);
            MenuExit = 1;
        }
    }    
    ShowChannels();
    myDelay(1200);
}

void MenuSetTimeAdjust() {
    snprintf(LCDbuff0, 17, "Set the value of");
    snprintf(LCDbuff1, 17, "time adjust: %3d", TimeAdj);
    LCDbprint(3);
    //myDelay(350);
    boolean changeDisp = false;
    unsigned long TimeStart=millis();
    for(uint8_t MenuExit = 0; MenuExit < 1 ; ) {
        //Read_Key();
        if (myButt0.isClick()) { // Right
            EEPROM.write(10, TimeAdj);
    		    KeyTone();
            MenuExit = 1;
        } 
        if (myButt1.isClick()) { //Up
            if (TimeAdj < 99) TimeAdj++;
            else tone(TonePin, NOTE_E7, 40);
    		    changeDisp = true;
        } 
        if (myButt2.isClick()) { // Down
            if (TimeAdj > -99) TimeAdj--;
            else tone(TonePin, NOTE_E7, 40);
    		    changeDisp = true;
        } 
        if (myButt3.isClick()) { // Left
            TimeAdj = EEPROM.read(10);
    		KeyTone();
            MenuExit = 1;
        } 
        if (myButt4.isClick()) { // Menu
            EEPROM.write(10, TimeAdj);
    		KeyTone();
            MenuExit = 1;
        } // End key read
      	if (changeDisp) {
        		KeyTone();
        		snprintf(LCDbuff1, 17, "time adjust: %3d", TimeAdj);
        		LCDbprint(2);
        		changeDisp = false;
        		TimeStart=millis();
        }
        if((millis()-TimeStart) > 15000) {
          TimeAdj = EEPROM.read(10);
          MenuExit = 1;
        }
    }    
}

void BackLightDisp(uint8_t CursPos) {
    Timer1.setPwmDuty(BackLightPin, BLset[CursPos]);
	
    lcd.setCursor(0, CursPos);
    lcd.write(setpic);
    lcd.setCursor(0, 1 - CursPos);
    lcd.write(32);
    for (uint8_t i=0;i<2;i++) {
        lcd.setCursor(5, i);
        for (uint8_t j=0; j<12; j++) {
            if (92*j+11<=BLset[i]) lcd.write(255);
            else lcd.write(219); 
        }
    }    
}

void MenuBackLightSet() {
  	uint8_t LastItem = 0;
  	SubMenuDisp(LastItem, 3);
  	lcd.noBlink();
  	BackLightDisp(LastItem);
  	//myDelay(500);
  	boolean changeDisp = false;
  	unsigned long TimeStart=millis();
  	for(uint8_t MenuExit = 0; MenuExit < 1 ; ) {
      	//Read_Key();
		if (myButt0.isClick()) { // Right
			if (BLset[LastItem] <= 931) {
				BLset[LastItem] += 92;
				changeDisp = true;
			} else BLset[LastItem] = 1023;
		} 
		if (myButt1.isClick()) { //Up
			LastItem = 0;
			changeDisp = true;
		} 
		if (myButt2.isClick()) { // Down
			LastItem = 1;
			changeDisp = true;
		} 
		if (myButt3.isClick()) { // Left
			if (BLset[LastItem] >= 103) {
				BLset[LastItem] -= 92;
				changeDisp = true;
			} else BLset[LastItem] = 0;
		} 
		if (myButt4.isClick()) { // Menu
			KeyTone();
			EEPROM.write(11, BLset[0]/4);
			EEPROM.write(12, BLset[1]/4);
			Timer1.setPwmDuty(BackLightPin, 1023);
			MenuExit = 1;
		} // End key read
		if (changeDisp) {
			KeyTone();
			BackLightDisp(LastItem);
			changeDisp = false;
			TimeStart=millis();
		}
		if((millis()-TimeStart) > 15000) {
			if (EEPROM.read(11) > 0) {
					BLset[0] = 4*EEPROM.read(11);
					BLset[1] = 4*EEPROM.read(12);
			} else {
					BLset[0] = 1023;
					BLset[1] = 220;
			}
			Timer1.setPwmDuty(BackLightPin, 1023);
			MenuExit = 1;
		}
  	}    
}

void MenuDisp(uint8_t CursPos, uint8_t DispPos) { // Display main menu
    char MenuItem[5][17] = {" Set date & time", " Setting timers ", " Feeding task   ", " System menu    ", " Menu exit      "};
    snprintf(LCDbuff0, 17, MenuItem[DispPos]);
    snprintf(LCDbuff1, 17, MenuItem[DispPos+1]);
    LCDbprint(3);
    lcd.setCursor(0, (CursPos-DispPos));
    lcd.write(setpic);
}

void MenuSelect() { // main menu function
    unsigned long TimeStart=millis();
    uint8_t LastItem = 0;
    uint8_t LastDisp = 0;
    uint8_t MaxItem = 4; // 5 items max
    Timer1.setPwmDuty(BackLightPin, 1023);
    lcd.noCursor();
    lcd.noBlink();
    boolean changeDisp = 0;
    MenuDisp(LastItem, LastDisp);
    //myDelay(500);
    for (uint8_t MenuExit = 0; MenuExit < 1;) {
        //Read_Key();
        if (myButt0.isClick()) { // Right
            switch( LastItem ) {
                case 0: // * >Set Date&Time *
                    KeyTone();
                    TimeSetup(); 
                    MenuExit = 1;
                    break;
                case 1: // *  Set Timers    *
                    KeyTone();
                    SetTimers();
                    changeDisp = true;
                    break;
                case 2: // *  Set eat time  *
                    KeyTone();
                    FeedMenu();
                    changeDisp = true;
                    break;
                case 3: // *  System menu   *
                    KeyTone();
                    SysMenuSelect();
                    changeDisp = true;
                    break;
                case 4: // *  Exit menu     *
                    KeyTone();
                    MenuExit = 1;
                    break;
            }
            TimeStart=millis()-15000;
        }
        if (myButt1.isClick() || myButt1.isStep()) { // Up
            if (LastItem > 0) {
                if (LastItem <= LastDisp) LastDisp--;
                LastItem--;
            }
            changeDisp = true;
        }
        if (myButt2.isClick() || myButt2.isStep()) { // Down
            if (LastItem < MaxItem) {
                if (LastItem > LastDisp) LastDisp++;
                LastItem++;
            }
            changeDisp = true;
        }
        if (myButt3.isClick()) { // Left
            LastItem = MaxItem;
            LastDisp = MaxItem - 1;
            changeDisp = true;
        }
        if (myButt4.isClick()) { // Menu
            KeyTone();
            MenuExit = 1;
        }
        if (changeDisp) {
            KeyTone();
        	TimeStart=millis();
            MenuDisp(LastItem, LastDisp);
            changeDisp = false;
        }
        if((millis()-TimeStart) > 45000) MenuExit = 1;
    }
}

void SysMenuDisp(uint8_t CursPos, uint8_t DispPos) { // Demonstration of the additional menu
    char MenuItem[8][17] = {" Set hour beep  ", " Set key tone   ", " Set relay type ", " Set num channel", " Set time adjust", " Set backlight  ", " Default value  ", " Main menu      "};
    snprintf(LCDbuff0, 17, MenuItem[DispPos]);
    snprintf(LCDbuff1, 17, MenuItem[DispPos+1]);
    LCDbprint(3);
    lcd.setCursor(0, (CursPos-DispPos));
    lcd.write(setpic);
}

void SysMenuSelect() { // System menu
    uint8_t LastItem = 0;
    uint8_t LastDisp = 0;
    uint8_t MaxItem = 7; // 8 items max
    lcd.noCursor();
    lcd.noBlink();
    SysMenuDisp(LastItem, LastDisp);
    //myDelay(500);
    boolean changeDisp = false;
    unsigned long TimeStart=millis();
    for(uint8_t MenuExit = 0; MenuExit < 1; ) {
        //Read_Key();
        if (myButt0.isClick()) { // Right
            changeDisp = true;
            switch( LastItem ) { 
                case 0: // * >Hour beep on  *
                  SubChangeMenu(1); 
                  break;
                case 1: // *  Key tone on   *
                  SubChangeMenu(2); 
                  break;
                case 2: // *  Set relay type *
                  SubChangeMenu(0);
                  break;
                case 3: // *  Set num channel*
                  MenuChannelsNum();
                  break;
                case 4: // *  Set time adjust value*
                  MenuSetTimeAdjust();
                  break;
                case 5: // *  Set backlight value*
                  MenuBackLightSet();
                  break;
                case 6: // *  Default value *
                  EEPROM.write(0,1);
                  ReadWriteEEPROM();
                  MenuExit = 1;
                  break;
                case 7: // *  Exit menu     *
                  MenuExit = 1;
                  break;
            }
        } 
        if (myButt1.isClick()) { //Up
            changeDisp = true;
            if (LastItem > 0) {
                if (LastItem <= LastDisp) LastDisp--;
                LastItem--;
            }
        } 
        if (myButt2.isClick()) { // Down
            changeDisp = true;
            if (LastItem < MaxItem) {
                if (LastItem > LastDisp) LastDisp++;
                LastItem++;
            }
        } 
        if (myButt3.isClick()) { // Left
            changeDisp = true;
    		    LastItem = MaxItem;
            LastDisp = MaxItem - 1;
        } 
        if (myButt4.isClick()) { // Menu
            KeyTone();
    		    MenuExit = 1;
        } // End key read
      	if (changeDisp) {
        		KeyTone();
        		SysMenuDisp(LastItem, LastDisp);
        		changeDisp = false;
        		TimeStart=millis();
      	}
        if((millis()-TimeStart) > 45000) MenuExit = 1;
    }
}

boolean NotFeeding(uint16_t CurrTime) { // Verification of the need for feeding
    uint8_t ttt = 1;
#ifdef TEST_MODE
    ttt = 5;
#endif
    if (((CurrTime >= FeedTime1) && (CurrTime < (FeedTime1+ttt))) || ((CurrTime >= FeedTime2) && (CurrTime < (FeedTime2+ttt)))) {
        if (FeedOK) return true;
        else {
            FeedOK = true;
            return false;
        }
    } else {
        FeedOK = false;
        return true;
    }
}

void myDelay(uint16_t deltm) {
    unsigned long TimeStop = deltm + millis();
    while (millis() < TimeStop);      
}

void FeedStart() {
    lcd.print("Feeding");
    if (BeepOnOff) {
        tone(TonePin, NOTE_A5, 100);
        myDelay(120);
        tone(TonePin, NOTE_A5, 500);
    }
    digitalWrite(VibroPin, HIGH);
    myDelay(FeedDelay*100);
    digitalWrite(VibroPin, LOW);
    myDelay(500);
}

void TimeAdjusting() { // Time adjusting
    DateTime future (rtc.now() + TimeSpan(0,0,0,TimeAdj));
    rtc.adjust(future);
    if (TimeAdj < 0) {
        lcd.print("Adjusting");
        lcd.setCursor(0, 1);
        myDelay((1+abs(TimeAdj))*1000);
    }
}

void loop() { // Main sketch loop
#ifndef TEST_MODE
    if (clockTimer.isReady()) {
        t = rtc.now(); // Get data from the DS1307
        if (oldsec != t.second()) {
            oldsec = t.second();
            snprintf(LCDbuff0, 17, "%3s, %02d %3s %04d", dn[t.dayOfTheWeek()], t.day(), mn[t.month()-1], t.year());
            LCDbprint(1);
            lcd.setCursor(0, 1);
            TimeNow = t.hour()*60+t.minute();
            if ((TimeNow == 210) && (t.second() == 15) && (TimeAdj != 0)) TimeAdjusting();
            if (NotFeeding(TimeNow)) {
                snprintf(LCDbuff1, 17, "%02d:%02d:%02d \0", t.hour(), t.minute(), t.second());
                LCDbprint(2);
                if (!BLNightState) HourBeep(TimeNow);
            } else FeedStart();
        } 
    }  
    if (rotateTimer.isReady()) {
        lcd.setCursor(9, 1);
        lcd.print(TimerOnOffDisp(StateChannels(TimeNow, t.dayOfTheWeek())));
    }
    //if (keypadTimer.isReady()) {
        //Read_Key();
    if (myButt0.isClick()) { // Right
        if (Ch2OnOff) {
          if (!Ch2NeedOn) Ch2NeedOff = true;
          else Ch2NeedOn = false;
        } else {
          if (!Ch2NeedOff) Ch2NeedOn = true;
          else Ch2NeedOff = false;
        }
        KeyTone();
    }
    if (myButt1.isClick()) { // Up
        if (BLNightState == 0) {
          if (!BLNeedOn) BLNeedOff = true;
          else BLNeedOn = false;
        } else {
          if (!BLNeedOff) BLNeedOn = true;
          else BLNeedOff = false;
        }
        KeyTone();
    }
    if (myButt2.isClick()) { // Down
        KeyTone();
        lcd.setCursor(9, 1);
        FeedStart();
    }
    if (myButt3.isClick()) { // Left
        if (Ch1OnOff) {
          if (!Ch1NeedOn) Ch1NeedOff = true;
          else Ch1NeedOn = false;
        } else {
          if (!Ch1NeedOff) Ch1NeedOn = true;
          else Ch1NeedOff = false;
        }
        KeyTone();
    }
    if (myButt4.isClick()) { // Menu
        KeyTone();
        MenuSelect();
    }
#else // for test mode
    for (uint8_t dw = 1; dw < 8 ; dw++) {
        for (uint16_t ct = 0 ; ct < 12*24 ; ct++) {
            lcd.setCursor(0, 0);
            lcd.print(dn[dw-1]);
            lcd.print(", 1");
            lcd.print(dw-1);           
            lcd.print(" Sep 2017");   
            lcd.setCursor(0, 1);
            lcd.print(lid1Zero(ct/12));
            lcd.write(58); // :
            lcd.print(lid1Zero((ct*5)%60));
            lcd.print(":00 ");
            if (NotFeeding(5*ct)) {
              lcd.print(TimerOnOffDisp(StateChannels(5*ct, dw)));
              if (!BLNightState) HourBeep(5*ct);
            } else FeedStart();
            myDelay(50);
            //Read_Key();
            if (myButt0.isClick()) { // Right
                KeyTone();
                if (Ch2OnOff) {
                    if (!Ch2NeedOn) Ch2NeedOff = true;
                    else Ch2NeedOn = false;
                } else {
                    if (!Ch2NeedOff) Ch2NeedOn = true;
                    else Ch2NeedOff = false;
                }
                myDelay(500);
            }
            if (myButt1.isClick()) { // Up
                KeyTone();
                if (BLNightState == 0) {
                    if (!BLNeedOn) BLNeedOff = true;
                    else BLNeedOn = false;
                } else {
                    if (!BLNeedOff) BLNeedOn = true;
                    else BLNeedOff = false;
                }
                myDelay(200);
            }
            if (myButt2.isClick()) { // Down
                KeyTone();
                lcd.setCursor(9, 1);
                FeedStart();
            }
            if (myButt3.isClick()) { // Left
                KeyTone();
                if (Ch1OnOff) {
                    if (!Ch1NeedOn) Ch1NeedOff = true;
                    else Ch1NeedOn = false;
                } else {
                    if (!Ch1NeedOff) Ch1NeedOn = true;
                    else Ch1NeedOff = false;
                }
                myDelay(500);
            }
            if (myButt4.isClick()) { // Menu
                KeyTone();
                MenuSelect();
            }
        }
    }
#endif
}
