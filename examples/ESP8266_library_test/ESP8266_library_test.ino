#include <SoftwareSerial.h>
#include <ESP8266wifi.h>

SoftwareSerial swSerial(8, 9, false);

// 10 is the pin used to reset the esp8266 
/ the last parameter sets the local echo option for the ESP8266 module..
ESP8266wifi wifi(swSerial, swSerial, 10, true); 

String inputString;
boolean stringComplete = false;
unsigned long nextPing = 0;

void setup() {
  inputString.reserve(20);
  pinMode(6, INPUT); //RX
  pinMode(7, OUTPUT); //TX

  swSerial.begin(9600);

  Serial.begin(9600);
  while (!Serial)
    ;
    
  //Set config options
  wifi.debug(Serial); //Will print everything from esp8266 to Serial stream of you choice
  // wifi.setTransportToUDP();//Will use UDP when connecting to server, default is TCP
  // wifi.setTransportToTCP();//Will change back to using TCP
  wifi.endSendWithNewline(true); // Will end all transmissions with a newline and carrage return ie println.. default is true
  
  wifi.begin();
  wifi.connectToAP("AP_SSID", "AP_PASSWORD");
  wifi.connectToServer("192.168.5.123", "2121");
  wifi.send(SERVER, "ESP8266 test app started");
}

void loop() {
  if (!wifi.isStarted())
      wifi.begin();
  /*
  //Turn on local ap and server if we are not connected, otherwise turn off (at this time, local server is TCP only)
   if(!wifi.isConnectedToServer() && !wifi.isLocalAPAndServerRunning())
   wifi.startLocalAPAndServer("MY_CONFIG_AP", "password", "5", "2121");
   else if(wifi.isConnectedToServer() && wifi.isLocalAPAndServerRunning())
   wifi.stopLocalAPAndServer();
   */

  static char buf[20];
  if (stringComplete) {
    inputString.toCharArray(buf, sizeof buf);
    wifi.send(SERVER, buf);
    inputString = "";
    stringComplete = false;
    nextPing = millis() + 10000;
  }

  //Send a ping once in a while..
  if (millis() > nextPing) {
    wifi.send(SERVER, "Ping ping..");
    nextPing = millis() + 10000;
  }

  //Listen for responses
  WifiMessage in = wifi.listenForIncomingMessage(6000);
  if (in.hasData) {
    Serial.println(in.message);

    Serial.print(freeMemoryAsInt());
    Serial.println(" bytes free");
    //Echo back;
    wifi.send(in.channel, "ES:", false);
    wifi.send(in.channel, in.message);
    nextPing = millis() + 10000;
  }
}

void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}





































