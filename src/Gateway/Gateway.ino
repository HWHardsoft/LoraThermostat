// LoRa WiFi Gateway for Lora room thermostate 
// (c) Zihatec GmbH, 2023
// Autor: Hartmut Wendt
// Version 01-00 


#include <SPI.h>
#include <RH_RF95.h>
#include <WiFi.h> 
#include <WiFiClientSecure.h>  
#include <HTTPClient.h>
#include "settings.h"


//Google Scripts setup
const char* host = "script.google.com";
//const char* googleRedirHost = "script.googleusercontent.com";
const char* googleRedirHost = "script.google.com";
const int httpsPort = 443;
const int maxStringLen = 20; //Maximum length of string for fetching
//String url = String("/macros/s/") + GScriptId + "/exec";
String url = String("/macros/s/") + GScriptId + "/dev";
const String delimeter = "\n"; //Delimiter for Events


// Port definitions
#define RFM95_CS   33  // "B"
#define RFM95_INT  27  // "A"
#define RFM95_RST  13
#define LED_BLUE   14  // blue led: fetching ok	
#define LED_GREEN  12  // power led

WiFiClientSecure client;
HTTPClient http;

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 434.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

byte radio_payload = 0;

void setup() {
  // Pin definition
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);

  // debug Schnittstelle
  Serial.begin(115200);
  //while (!Serial) delay(1);
  delay(1500);


  //CONNECTION SETUP
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.begin(ssid, password);

  Serial.print(F("Connecting to WiFi: "));
  Serial.println(ssid);
  Serial.print(F("Please wait..."));

  //WAIT FOR CONNECTION
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println(F("Wifi connected!"));


  Serial.println("Feather LoRa TX Init!");

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println(F("LoRa radio init failed"));
    Serial.println(F("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info"));
    while (1);
  }
  Serial.println(F("LoRa radio init OK!"));

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println(F("setFrequency failed"));
    while (1);
  }
  Serial.print(F("Set Freq to: ")); Serial.println(RF95_FREQ);

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);

  // switch blue LED on
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, LOW);
}

int16_t packetnum = 0;  // packet counter, we increment per xmission

void loop() {
  delay(60000); // Wait 60 seconds between transmits, could also 'sleep' here!
  digitalWrite(LED_BLUE, HIGH);
  Serial.println(F("Transmitting...")); // Send a message to rf95_server

  radio_payload =  fetchDataFromGoogle(); //get data from Google Calendar;
  if ((radio_payload >= 0) && (radio_payload <16)) {
    char radiopacket[5] = "    ";
    radiopacket[0] = radio_payload;
    radiopacket[1] = 0;

    Serial.print(F("Sending 0x0")); Serial.print(radio_payload, HEX);
    Serial.print(F(", Packet number ")); Serial.println(packetnum++);
  

    Serial.println("Sending...");
    delay(10);
    rf95.send((uint8_t *)radiopacket, 1 );

    Serial.print(F("Waiting for packet to complete..."));
    delay(10);
    rf95.waitPacketSent();
  
    Serial.println(F("done!"));
    digitalWrite(LED_BLUE, LOW);
  } else {
    Serial.println(F("No valid data for transmittion!"));
    delay(1000);
    // reset ESP32
    ESP.restart();
  } 
}


//Get entries data through Google Script
int fetchDataFromGoogle() {
   http.end();
   http.setTimeout(20000);
   http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  
   Serial.print(F("Fetch data from google script: "));
   Serial.print(host + url);
   if (!http.begin("https://script.google.com/macros/s/your link data/exec")) {
    Serial.println(F("Cannot connect to google script"));
    return (-1);
   } 

   Serial.println(F("Connected to google script"));
   int returnCode = http.GET();
   Serial.print(F("Returncode: ")); Serial.println(returnCode);
   String response = http.getString();
   Serial.print(F("Response: ")); Serial.println(response);
   byte return_code = 0;
   
   response.toLowerCase();

   if (response.indexOf("current events") >= 0) {    
     if (response.indexOf("room1") >= 0) return_code |= 0x01;
     if ((response.indexOf("room2") >= 0) return_code |= 0x02;
     if (response.indexOf("auto") >= 0) return_code |= 0x08; //automatic switch off
   }
   return(return_code);
}
