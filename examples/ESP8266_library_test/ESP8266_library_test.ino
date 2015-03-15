#include <SoftwareSerial.h>
#include <ESP8266wifi.h>

#define sw_serial_rx_pin 4
#define sw_serial_rx_pin 6
#define esp8266_reset_pin 5 // Connect this pin to CH_PD on the esp8266, not reset. (let reset be unconnected)

SoftwareSerial swSerial(sw_serial_rx_pin, sw_serial_rx_pin);

// the last parameter sets the local echo option for the ESP8266 module..
ESP8266wifi wifi(swSerial, swSerial, esp8266_reset_pin, true);

String inputString;
boolean stringComplete = false;
unsigned long nextPing = 0;

void setup() {
  inputString.reserve(20);
  swSerial.begin(9600);
  Serial.begin(9600);
  while (!Serial)
    ;
  Serial.println("Starting wifi");

  //Set config options
  wifi.debug(Serial); //Will print everything from esp8266 to Serial stream of you choice

  wifi.setTransportToTCP();// this is also default
  // wifi.setTransportToUDP();//Will use UDP when connecting to server, default is TCP

  wifi.endSendWithNewline(true); // Will end all transmissions with a newline and carrage return ie println.. default is true

  wifi.begin();
  wifi.connectToAP("ComHem719F6D", "kufp7dy7");
  wifi.connectToServer("192.168.0.28", "2121");
  wifi.send(SERVER, "ESP8266 test app started");
}

void loop() {
  
  //Make sure the esp8266 is started..
  if (!wifi.isStarted())
    wifi.begin();
    
  //Send what you typed in the arduino console to the server
  static char buf[20];
  if (stringComplete) {
    inputString.toCharArray(buf, sizeof buf);
    wifi.send(SERVER, buf);
    inputString = "";
    stringComplete = false;
  }

  //Send a ping once in a while..
  if (millis() > nextPing) {
    wifi.send(SERVER, "Ping ping..");
    nextPing = millis() + 10000;
  }

  //Listen for incoming messages and echo back, will wait until a message is received, or max 6000ms..
  WifiMessage in = wifi.listenForIncomingMessage(6000);
  if (in.hasData) {
    Serial.println(in.message);
    //Echo back;
    wifi.send(in.channel, "ES:", false);
    wifi.send(in.channel, in.message);
    nextPing = millis() + 10000;
  }
}

//Listen for serial input from the console
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}





































