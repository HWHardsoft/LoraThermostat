/*﻿
 *  Application note: Lora Thermostat
 *  for Cortex M0
 *  Version 1.1
 *  Copyright (C) 2023  Hartmut Wendt  www.zihatec.de
 *  
 * 
 *  
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/   


/*______Import Libraries_______*/
#include <Arduino.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <XPT2046_Touchscreen.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "usergraphics.h"
#include <FlashAsEEPROM.h>
//#include <EEPROM.h>
/*______End of Libraries_______*/


// für Winterkirche 
//#define __Winterkirche__

/*__Select your hardware version__*/
// select one version and deselect the other versions
// #define _AZ_TOUCH_ESP            // AZ-Touch ESP
// #define _AZ_TOUCH_MOD_SMALL_TFT  // AZ-Touch MOD with 2.4 inch TFT
#define _AZ_TOUCH_MOD_BIG_TFT    // AZ-Touch MOD with 2.8 inch TFT

/*____End of hardware selection___*/


// RFM9x pin definitions
  #define RFM95_CS    8
  #define RFM95_INT   3
  #define RFM95_RST   4

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 434.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

 


//______Define LCD pins for ArduiTouch _______
  #define TFT_CS   10
  #define TFT_DC   9
  #define TFT_LED  13

  #define TOUCH_CS 6
  #define TOUCH_IRQ 5 

  #define BEEPER 11
//_______End of definitions______


// other pins
  #define RELAY    17 //A3
  #define SENSOR   15 //A1

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(SENSOR);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

/*
//__Pin definitions for the feather M0__

  #define TFT_CS   33
  #define TFT_DC   15
  #define TFT_LED  13

  #define TOUCH_CS 32
  #define TOUCH_IRQ 14 

  #define BEEPER 27

//_______End of definitions______

*/

/*______Assign pressure_______*/
#define ILI9341_ULTRA_DARKGREY    0x632C      
#define MINPRESSURE 100
#define MAXPRESSURE 2000
#define DISPLAY_ON_TIME 10000
/*_______Assigned______*/

/*____Calibrate TFT LCD_____*/
#define TS_MINX 370
#define TS_MINY 470
#define TS_MAXX 3700
#define TS_MAXY 3600
/*______End of Calibration______*/




/*____Program specific constants_____*/
#define MAX_TEMPERATURE 25  
#define MIN_TEMPERATURE 12
enum { PM_MAIN, PM_OPTION, PM_CLEANING};  // Program modes
enum { BOOT, COOLING, TEMP_OK, HEATING};  // Thermostat modes
#define TIMEOUT_RSSI 12000    // 2 minutes
#define TIMEOUT_1WIRE 3000    // 30sec
/*______End of specific constants______*/


#define eep_addr_heiztemp 0
#define eep_addr_absenktemp 1

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
XPT2046_Touchscreen touch(TOUCH_CS);

// enable this option for debugging via UART (the MODBUS funtion is disabled in this case)
#define _debug

 int X,Y;
 uint8_t Thermostat_mode = BOOT;
 
 uint8_t iAbsenk_temperatur;   // lowering temperature in automatic mode
 uint8_t iHeiz_temperatur;     // heating temperature in automatic mode
 
 uint8_t iRoom_temperature = 21;    // measured temperature
 uint8_t iSet_temperature = 20;     // manually adjusted temperature

 uint8_t PMode = PM_MAIN;         // program mode
 bool Touch_pressed = false;
 uint8_t Timer_Cleaning=0;

 uint16_t display_on_tmr = DISPLAY_ON_TIME;
 boolean BAutomatic = true;  // automatic mode
 boolean BRemoteHeatingON = false; // set heating mode on via Google calendar
 int16_t iRSSI = 0; // measured field strength
 int iTimer_RSSI = 0; 
 uint16_t iTimer_1WIRE = 0; 
 byte bLoRa_data = 0;



void setup() {
  Serial.begin(115200); 
  
  // port definitions
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, LOW);    // LOW to turn backlight on - pcb version 01-02-00

  // EEPROM
  iHeiz_temperatur = EEPROM.read(eep_addr_heiztemp);
  if ((iHeiz_temperatur > MAX_TEMPERATURE) || (iHeiz_temperatur < MIN_TEMPERATURE)) iHeiz_temperatur = MAX_TEMPERATURE;
  iAbsenk_temperatur = EEPROM.read(eep_addr_absenktemp);
  if ((iAbsenk_temperatur > MAX_TEMPERATURE) || (iAbsenk_temperatur < MIN_TEMPERATURE)) iAbsenk_temperatur = MIN_TEMPERATURE;

  // temperature Sensor
  Serial.println("1Wire temperature sensor init!");  
  delay(50);
  sensors.begin();
  read_sensor();

  // RFM9x
  Serial.println("Feather LoRa RX Init!");

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
    while (1);
  }
  Serial.println("LoRa radio init OK!");


  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

  // touchscreen
  tft.begin();
  touch.begin();
  Serial.print("tftx ="); Serial.print(tft.width()); Serial.print(" tfty ="); Serial.println(tft.height());  

  
  set_auto_temperature();
  draw_main_screen();
  
}


TS_Point p;

void loop() {
  if (Touch_Event()== true) { 
    X = p.y; Y = p.x;
    if (Touch_pressed == false) {
      if (display_on_tmr) DetectButtons();
      display_on_tmr = DISPLAY_ON_TIME; // reset BL timer       
    }
    Touch_pressed = true;
    // Serial.println("touch event");
   
  } else {
    Touch_pressed = false;
    
 }

  //automatic display BL timeout
  if (display_on_tmr) {
    display_on_tmr--;
    digitalWrite(TFT_LED, LOW);    // LOW to turn backlight on - pcb version 01-02-00
    	   
  } else {
    digitalWrite(TFT_LED, HIGH);    // HIGH to turn backlight off - pcb version 01-02-00
    if (PMode != PM_MAIN) {
      PMode = PM_MAIN;
      draw_main_screen(); 
    }        
  }

  // operate LoRa input
  byte old_lora_data = bLoRa_data;
  int i1 = LoRa_input();
  if ((i1 >= 0) && (i1 < 16)) bLoRa_data = i1;
  // Heizung Ein oder AUS über Remote
  #if defined (__Winterkirche__)
     
    if (bLoRa_data & 0x2) {
      BRemoteHeatingON = true;
    } else {
      BRemoteHeatingON = false;      
    }
    
  #else
    
    if (bLoRa_data & 0x1) {
      BRemoteHeatingON = true;
    } else {
      BRemoteHeatingON = false;
    } 
    
  #endif
  
  set_auto_temperature();
  if ((old_lora_data != bLoRa_data) && (PMode == PM_MAIN)) {
     update_SET_temp();
     draw_circles();
  } 
  
  // switch to Automatic mode ??
  if (bLoRa_data & 0x8) {
    BAutomatic = true;
    set_auto_temperature();
    update_auto_button();
    draw_circles();
  }

  // measure temperatur if backlight is off
  if (digitalRead(TFT_LED) == HIGH) {
    if (iTimer_1WIRE < TIMEOUT_1WIRE) {
      iTimer_1WIRE++;
    } else {
      read_sensor();
      iTimer_1WIRE = 0;
      draw_circles();
    }  
  }


  // Relay control
  relay_control();
   
  // screen cleaning
  Cleaning_processing();
  delay(10);
  
}


/********************************************************************//**
 * @brief     detects a touch event and converts touch data 
 * @param[in] None
 * @return    boolean (true = touch pressed, false = touch unpressed) 
 *********************************************************************/
bool Touch_Event() {
  p = touch.getPoint(); 
  delay(1);
  #ifdef _AZ_TOUCH_MOD_BIG_TFT
    // 2.8 inch TFT with yellow header
    p.x = map(p.x, TS_MINX, TS_MAXX, 320, 0); 
    p.y = map(p.y, TS_MINY, TS_MAXY, 0, 240);
  #else
    // 2.4 inch TFT with black header
    p.x = map(p.x, TS_MINX, TS_MAXX, 0, 320); 
    p.y = map(p.y, TS_MINY, TS_MAXY, 240, 0);
  #endif  
  if (p.z > MINPRESSURE) return true;  
  return false;  
}


/********************************************************************//**
 * @brief     Processing for screen cleaning function
 * @param[in] None
 * @return    None
 *********************************************************************/
void Cleaning_processing()
{
  // idle timer for screen cleaning
  if (PMode == PM_CLEANING) {
      if ((Timer_Cleaning % 10) == 0) {
        tft.fillRect(0,0, 100, 60, ILI9341_BLACK);
        tft.setCursor(10, 50);
        tft.print(Timer_Cleaning / 10);
        
      }
      if (Timer_Cleaning) {
        Timer_Cleaning--;
        delay(100);
      } else {
        draw_option_screen();
        PMode = PM_OPTION;
      }
  }  
}



/********************************************************************//**
 * @brief     detecting pressed buttons with the given touchscreen values
 * @param[in] None
 * @return    None
 *********************************************************************/
void DetectButtons()
{
  // in main program
  if (PMode == PM_MAIN){

   // button Plus
   if ((X>186) && (Y>260)) {
    if (iSet_temperature < MAX_TEMPERATURE) iSet_temperature++;
    update_SET_temp();
    update_circle_color();
    if (BAutomatic) {
      BAutomatic = false;      
      update_auto_button();    
    }
   }
    
   // button Minus
   if ((X<54) && (Y>260)) {
    if (iSet_temperature > MIN_TEMPERATURE) iSet_temperature--;
    update_SET_temp();
    update_circle_color();
    if (BAutomatic) {
      BAutomatic = false;      
      update_auto_button();    
    }
   }

   // button auto
   if ((X>55) && (X<185) && (Y>260)) {
      if (BAutomatic == false) {
        BAutomatic = true;
        set_auto_temperature();
        update_auto_button();
        update_SET_temp();
        update_circle_color();
      }  
   }

   // button gearwheel
   if ((X<60) && (Y<50)) {
    draw_option_screen();
    PMode = PM_OPTION;
   }


 
  } else if (PMode == PM_OPTION){ 
   
   // button - Heiztemperatur
   if ((X<110) && (Y<75)) {
    if (iHeiz_temperatur >  MIN_TEMPERATURE) iHeiz_temperatur--;
    update_Heiztemperatur();
   }
  
   // button + Heiztemperatur
   if ((X>130) && (Y<75)) {
    if (iHeiz_temperatur < MAX_TEMPERATURE) iHeiz_temperatur++;
    update_Heiztemperatur();
   }

   // button - Absenktemperatur
   if ((X<110) && (Y<155) && (Y>85)) {
    if (iAbsenk_temperatur >  MIN_TEMPERATURE) iAbsenk_temperatur--;
    update_Absenktemperatur();
   }
  
   // button + Absenktemperatur
   if ((X>130) && (Y<155) && (Y>85)) {
    if (iAbsenk_temperatur < MAX_TEMPERATURE) iAbsenk_temperatur++;
    update_Absenktemperatur();
   }
   
   // button screen cleaning
   if ((Y>165) && (Y<238)) {
     tft.fillScreen(ILI9341_BLACK);
     tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
     tft.setFont(&FreeSansBold24pt7b);  
     PMode = PM_CLEANING;    
     Timer_Cleaning = 255;
   }


   // button OK
   if (Y>245) {
     EEPROM.write(eep_addr_heiztemp,iHeiz_temperatur);
     EEPROM.write(eep_addr_absenktemp,iAbsenk_temperatur);
     EEPROM.commit();
     delay(100);
     Thermostat_mode = BOOT;
     PMode = PM_MAIN;
     draw_main_screen();         
   }
    
    
  }
}



/********************************************************************//**
 * @brief     Drawing of the main program screen
 * @param[in] None
 * @return    None
 *********************************************************************/
void draw_main_screen()
{
  tft.fillScreen(ILI9341_BLACK);
  
  // draw circles
  update_circle_color();

  // draw temperature up/dwn buttons
  draw_buttons();

  // draw icons
  tft.drawRGBBitmap(10,10, wrench, 24,24);  
    

  update_SET_temp();

  // draw signal level
  update_signal_icon(iRSSI);
 
}



/********************************************************************//**
 * @brief     Drawing of the screen for Options menu
 * @param[in] None
 * @return    None
 *********************************************************************/
void draw_option_screen()
{
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&FreeSansBold9pt7b);  
  
  // Einstellung Heiztemperatur Automatikbetrieb
  tft.setCursor(10, 20);
  tft.print("Heiztemperatur");
  tft.setFont(&FreeSansBold24pt7b);
  tft.setCursor(30, 65);
  tft.print("-");
  tft.setCursor(190, 65);
  tft.print("+");
  tft.drawLine(5,80,235,80, ILI9341_WHITE);
  update_Heiztemperatur();


  // Einstellung Absenktemperatur Automatikbetrieb
  tft.setCursor(10, 100);
  tft.setFont(&FreeSansBold9pt7b);  
  tft.print("Absenktemperatur");
  tft.setFont(&FreeSansBold24pt7b);
  tft.setCursor(30, 145);
  tft.print("-");
  tft.setCursor(190, 145);
  tft.print("+");
  tft.drawLine(5,160,235,160, ILI9341_WHITE);
  update_Absenktemperatur();


  // Screen cleaning idle timer
  tft.setFont(&FreeSansBold12pt7b);  
  tft.setCursor(46, 205);
  tft.print("Putz Modus");
  //tft.drawLine(5,160,235,160, ILI9341_WHITE);

  // OK Button
  tft.setFont(&FreeSansBold24pt7b);
  tft.drawLine(5,240,235,240, ILI9341_WHITE);
  tft.setCursor(90, 300);
  tft.print("OK");  
  
 
}


/********************************************************************//**
 * @brief     update of the value for set temperature on the screen
 *            (in the big colored circle)
 * @param[in] None
 * @return    None
 *********************************************************************/
void update_SET_temp()
{
  int16_t x1, y1;
  uint16_t w, h;
  String curValue = String(iSet_temperature);
  int str_len =  curValue.length() + 1; 
  char char_array[str_len];
  curValue.toCharArray(char_array, str_len);
  tft.fillRect(70, 96, 60, 50, ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&FreeSansBold24pt7b);
  tft.getTextBounds(char_array, 80, 130, &x1, &y1, &w, &h);
  tft.setCursor(123 - w, 130);
  tft.print(char_array);
}



/********************************************************************//**
 * @brief     update of the value for room temperature on the screen
 *            (in the small grey circle)
 * @param[in] None
 * @return    None
 *********************************************************************/
void update_Room_temp()
{
  int16_t x1, y1;
  uint16_t w, h;
  String curValue = String(iRoom_temperature);
  int str_len =  curValue.length() + 1; 
  char char_array[str_len];
  curValue.toCharArray(char_array, str_len);
  tft.fillRect(36, 200, 30, 21, ILI9341_ULTRA_DARKGREY);
  tft.setTextColor(ILI9341_WHITE, ILI9341_ULTRA_DARKGREY);
  tft.setFont(&FreeSansBold12pt7b);
  tft.getTextBounds(char_array, 40, 220, &x1, &y1, &w, &h);
  tft.setCursor(61 - w, 220);
  tft.print(char_array);
}



/********************************************************************//**
 * @brief     update of the color of the big circle according the 
 *            difference between set and room temperature 
 * @param[in] None
 * @return    None
 *********************************************************************/
void update_circle_color()
{
  // HEATING 
  if ((iRoom_temperature < iSet_temperature) && (Thermostat_mode != HEATING)) {
    Thermostat_mode = HEATING;
    draw_circles();
  }

  // COOLING 
  if ((iRoom_temperature > iSet_temperature) && (Thermostat_mode != COOLING)) {
    Thermostat_mode = COOLING;
    draw_circles();
  }

  // Temperature ok 
  if ((iRoom_temperature == iSet_temperature) && (Thermostat_mode != TEMP_OK)) {
    Thermostat_mode = TEMP_OK;
    draw_circles();
  }
}


/********************************************************************//**
 * @brief     update of the value for Heiztempratur in the options menu 
 * @param[in] None
 * @return    None
 *********************************************************************/
void update_Heiztemperatur()
{
  tft.fillRect(60, 30, 120, 45, ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&FreeSansBold24pt7b);
  tft.setCursor(95, 65);
  tft.print(iHeiz_temperatur);
}


/********************************************************************//**
 * @brief     update of the value for Absenktempratur in the options menu 
 * @param[in] None
 * @return    None
 *********************************************************************/
void update_Absenktemperatur()
{
  tft.fillRect(60, 110, 120, 45, ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&FreeSansBold24pt7b);
  tft.setCursor(95, 145);
  tft.print(iAbsenk_temperatur);
}



/********************************************************************//**
 * @brief     drawing of the circles in main screen including the value 
 *            of room temperature
 * @param[in] None
 * @return    None
 *********************************************************************/
void draw_circles()
{
  if (PMode != PM_MAIN) return;
  //draw big circle 
  unsigned char i;
  if (iRoom_temperature < iSet_temperature) {
    // heating - red
    for(i=0; i < 10; i++) tft.drawCircle(120, 120, 80 + i, ILI9341_RED);
  } else if (iRoom_temperature > iSet_temperature) {
    // cooling - blue
    for(i=0; i < 10; i++) tft.drawCircle(120, 120, 80 + i, ILI9341_BLUE);    
  } else {
    // Temperature ok
    for(i=0; i < 10; i++) tft.drawCircle(120, 120, 80 + i, ILI9341_GREEN);       
  }

  //draw small 
  tft.fillCircle(60, 200, 40, ILI9341_ULTRA_DARKGREY);

  //draw °C in big circle
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(130, 100);
  tft.print("o");
  tft.setFont(&FreeSansBold24pt7b);
  tft.setCursor(140, 130);
  tft.print("C");

  // draw room and °C in small circle
  tft.setTextColor(ILI9341_WHITE, ILI9341_ULTRA_DARKGREY);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setCursor(75, 220);
  tft.print("C");
  tft.drawCircle(69,204, 2, ILI9341_WHITE);
  tft.drawCircle(69,204, 3, ILI9341_WHITE);
  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(35, 190);
  tft.print("Raum");
  update_Room_temp();

}



/********************************************************************//**
 * @brief     drawing of the both buttons for setting temperature up 
 *            and down
 * @param[in] None
 * @return    None
 *********************************************************************/
void draw_buttons()
{

  //minus button 
  tft.drawRoundRect(10,270,40,40,6, ILI9341_WHITE);
  tft.fillRect(20, 287, 20, 6, ILI9341_WHITE);
    
  //plus button
  tft.drawRoundRect(190,270,40,40,6, ILI9341_WHITE);
  tft.fillRect(200, 287, 20, 6, ILI9341_WHITE);
  tft.fillRect(207, 280, 6, 20, ILI9341_WHITE);

  // auto / manuell button
  tft.drawRoundRect(60,270,120,40,6, ILI9341_WHITE); 
  update_auto_button();

  

}

/********************************************************************//**
 * @brief     update of the automatic button in the main screen  
 *            
 * @param[in] None
 * @return    None
 *********************************************************************/
void update_auto_button() {
  // auto / manuell button
  if (PMode != PM_MAIN) return;
  tft.fillRect(65, 275, 110, 30, ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE,ILI9341_BLACK);
  tft.setFont(&FreeSansBold9pt7b);
  if (BAutomatic) {
    tft.setCursor(75, 295);
    tft.print("Automatik");  
  } else {
    tft.setCursor(88, 295);
    tft.print("Manuell");         
  }
}


void set_auto_temperature(){
  if (BAutomatic){
    if  (BRemoteHeatingON) {
      /*
      if ((iSet_temperature != iHeiz_temperatur) && (PMode == PM_MAIN)) {
        iSet_temperature = iHeiz_temperatur;     // Heiztemperatur im Automatikbetrieb
        update_SET_temp();
        update_circle_color();
      } */     
      iSet_temperature = iHeiz_temperatur;     // Heiztemperatur im Automatikbetrieb
    } else {
      /*
      if ((iSet_temperature != iAbsenk_temperatur) && (PMode == PM_MAIN)) {
        iSet_temperature = iAbsenk_temperatur;   // Absenktemperatur im Automatikbetrieb
        update_SET_temp();
        update_circle_color();
      } */
      iSet_temperature = iAbsenk_temperatur;   // Absenktemperatur im Automatikbetrieb
    }
  }  
}


/********************************************************************//**
 * @brief     update of the signal icon  in the main screen  
 *            
 * @param[in] Feldstaerke (-15 max, -95 min)
 * @return    None
 *********************************************************************/
void update_signal_icon(int Feldstaerke){
  if (PMode != PM_MAIN) return;
  if ((Feldstaerke > -40) && (Feldstaerke < 0)){
    tft.drawRGBBitmap(206,10, signal4, 24,24);
  } else if ((Feldstaerke <= -40) && (Feldstaerke > -70)){
    tft.drawRGBBitmap(206,10, signal3, 24,24);
  } else if ((Feldstaerke <= -70) && (Feldstaerke > -90)){
    tft.drawRGBBitmap(206,10, signal2, 24,24);
  } else if (Feldstaerke <= -90) {
    tft.drawRGBBitmap(206,10, signal1, 24,24);
  } else {
    tft.drawRGBBitmap(206,10, signal0, 24,24);
  } 
}


/********************************************************************//**
 * @brief     operate wireless LoRa input by RFM)x module  
 *            
 * @param[in] None
 * @return    received byte
 *********************************************************************/
int LoRa_input() {
  int incoming_data = -1;
  if (rf95.available()) {
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len)) {
      RH_RF95::printBuffer("Received: ", buf, len);
      Serial.print("Got: ");
      Serial.println((char*)buf);
      incoming_data = buf[0];
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);
      iRSSI = rf95.lastRssi();
      update_signal_icon(iRSSI);
      iTimer_RSSI=0;
    }  
  } else {
    // handle timeout for RSSI icon
    if (iTimer_RSSI < TIMEOUT_RSSI) {
      iTimer_RSSI++;
    } else {
      iRSSI = 0;
      update_signal_icon(iRSSI);
    }
  }
  return(incoming_data);   
}


/********************************************************************//**
 * @brief     read data from 1wire temprature sensor
 *            
 * @param[in] None
 * @return    None
 *********************************************************************/
void read_sensor(){
  sensors.requestTemperatures(); 
  //float temperatureC = sensors.getTempCByIndex(0);
  //Serial.print(temperatureC);
  iRoom_temperature = sensors.getTempCByIndex(0);
  Serial.print("Measured temperature: ");
  Serial.print(iRoom_temperature);
  Serial.println("ºC");
}


/********************************************************************//**
 * @brief     control thermostat relay output 
 *            
 * @param[in] None
 * @return    None
 *********************************************************************/
void relay_control() {
  if (iRoom_temperature < iSet_temperature) {
    // Relay ON    
    if (digitalRead(RELAY) == LOW) Serial.println("Relay ON");
    digitalWrite(RELAY, HIGH);
  } else {
    // Relay OFF
    if (digitalRead(RELAY) == HIGH) Serial.println("Relay OFF");
    digitalWrite(RELAY, LOW);
  }
  
}
