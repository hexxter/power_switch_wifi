/*
 * Timer.h https://github.com/hexxter/Timer
 * 
 */
#include <NeoPixelBus.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "Timer.h"

const char* ssid = "XXXXXXXXXXXXX";
const char* password = "YYYYYYYYYYYYYYYYYYYYYYYY";
MDNSResponder mdns;
String inString = "";

const int pins[] = { 12, 14 };
const int pins_size = sizeof(pins) / sizeof(int);

const int button[] = { 13, 16 };
const int button_size = sizeof(button) / sizeof(int);

Timer t;

#define AUTOOFF 120
#define LED 2
#define LEDFLASH 3

int timeout = AUTOOFF;

// Neopixel settings
const int numLeds = 1; // change for your setup
const int numberOfChannels = numLeds * 3; // Total number of channels you want to receive (1 led = 3 channels)
NeoPixelBus  leds = NeoPixelBus(numLeds, LED, NEO_GRB | NEO_KHZ800);

ESP8266WebServer server(80);

void handleRoot() {
  String message = "power switch !\nPINS: ";

  for (uint8_t i = 0; i < pins_size; i++) {
    message += " " + String(pins[i]);
  }
  message += "\nButtons: ";
  for (uint8_t i = 0; i < button_size; i++) {
    message += " " + String(button[i]);
  }
  server.send(200, "text/plain", message);
}


int time_tasks[button_size];
// state LOW/HIGH, pin num, auto off time in min
void doSwitch( bool state, int pin, int auto_off ) {

  uint8_t i;
  for(i = 0; i<pins_size; i++){
    if( pins[i] == pin ) break;
  }
  
  if ( state == LOW ) {
    digitalWrite(pin, HIGH);
    if( time_tasks[i] > -1 ){
      t.stop(time_tasks[i]);
      time_tasks[i] = -1;
    }
  } else {
    time_tasks[i] = t.pulseImmediate(pin, auto_off * 60 * 1000, LOW);
  }
}

void handlePin() {

  if ( server.args() < 1 ) {
    server.send(500, "text/plain", "wrong argument pin number not found");
    return;
  }

  if (server.method() == HTTP_POST) {
    if ( server.args() < 2 ) {
      server.send(500, "text/plain", "2 arguments needed");
      return;
    }

    if ( server.argName(0) != "pin" && server.argName(1) != "val" ) {
      server.send(500, "text/plain", "wrong arguments");
      return;
    }
    int ref_pin = server.arg(0).toInt();
    bool ok = false;
    for (uint8_t i = 0; i < pins_size; i++) {
      if ( pins[i] == ref_pin ) ok = true;
    }

    if ( ok ) {
      if ( server.arg(1).toInt() ) doSwitch( HIGH, ref_pin, timeout );
      else doSwitch(LOW, ref_pin, timeout);

      server.send(200, "text/plain", "ok" );
    } else {
      server.send(500, "text/plain", "pin not usable");
      return;
    }
  } else {
    if ( server.argName(0) != "pin" ) {
      server.send(500, "text/plain", "argument pin is missing");
      return;
    }

    int ref_pin = server.arg(0).toInt();
    bool ok = false;
    for (uint8_t i = 0; i < pins_size; i++) {
      if ( pins[i] == ref_pin ) ok = true;
    }
    if ( ok ) {
      if ( !digitalRead(ref_pin) ) server.send(200, "text/plain", "on" );
      else server.send(200, "text/plain", "off" );
      
    } else server.send(500, "text/plain", "pin not usable");

    return;
  }

}

void handleTimeout() {

  if (server.method() == HTTP_POST) {
    if ( server.args() < 1 ) {
      server.send(500, "text/plain", "1 arguments needed");
      return;
    }

    if ( server.argName(0) != "val" ) {
      server.send(500, "text/plain", "wrong arguments");
      return;
    }
    timeout = server.arg(0).toInt();
    server.send(200, "text/plain", "ok" );
   
  } else {
    server.send(200, "text/plain", String(timeout) );
  }

}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

uint32_t spikecounter[button_size];
bool execute[button_size];
void setup(void) {

  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");

  //pinMode(LED, OUTPUT);
  leds.Begin();
  leds.SetPixelColor(0, RgbColor(0,0,0));
  leds.Show();

  for (uint8_t i = 0; i < pins_size; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i] , HIGH);
  }
  for (uint8_t i = 0; i < button_size; i++) {
    pinMode(button[i], INPUT);
    spikecounter[i] = 0;
  }

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.on("/pin", handlePin);
  server.on("/timeout", handleTimeout);

  server.begin();
  Serial.println("HTTP server started");
}

int ledstate = 0;
void loop(void) {
  server.handleClient();
  t.update();

  for (uint8_t i = 0; i < button_size; i++) {
    //Serial.println( "stat: "+String(i)+" "+String(digitalRead(button[i])==LOW)+ " "+String(button[i]) );
    if(digitalRead(button[i]) == LOW && execute[i]) {
      if (spikecounter[i] >= 4000) {
        doSwitch( digitalRead(pins[i]), pins[i], timeout );
        Serial.println( "button pressed: " + String(button[i]) + " / "+String(!digitalRead(button[i]))+"\n" );
        //t.oscillate( LED, 1000, LOW, LEDFLASH*(i+1) );
        spikecounter[i] = 0;
        execute[i] = false;
      } else {
        spikecounter[i]++;
      }
    }
    if(digitalRead(button[i]) == HIGH && !execute[i]) execute[i] = true;
  }  
  if(!digitalRead(pins[0]) && digitalRead(pins[1]) && ledstate != 1 ){
    ledstate = 1;
    Serial.println( "LED 1" );
    leds.SetPixelColor(0, RgbColor(100,0,0));
    leds.Show();
  }else if(digitalRead(pins[0]) && !digitalRead(pins[1]) && ledstate != 2){
    ledstate = 2;
    Serial.println( "LED 2" );
    leds.SetPixelColor(0, RgbColor(0,100,0));
    leds.Show();
  }else if(!digitalRead(pins[0]) && !digitalRead(pins[1]) && ledstate != 3 ){
    ledstate = 3;
    Serial.println( "LED 3" );
    leds.SetPixelColor(0, RgbColor(0,0,100));
    leds.Show();
  }else if(digitalRead(pins[0]) && digitalRead(pins[1]) && ledstate != 0){
    ledstate = 0;
    Serial.println( "LED 0" );
    leds.SetPixelColor(0, RgbColor(0,0,0));
    leds.Show();
  }
}
