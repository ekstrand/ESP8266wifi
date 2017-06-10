# SerialESP8266wifi
A simple ESP8266 Arduino library with built in re-connect functionality.
* The ESP8266 is a dirtcheap wifimodule. I got mine for about 2.50 US including shipping at Aliexpress. Read about it here: https://nurdspace.nl/ESP8266
* An AT command reference can be found here: https://github.com/espressif/esp8266_at/wiki/AT_Description
* Contact me if you have ideas for changes or new features, found a bug or just want to buy a beer at jonas[AT]inspirativ[DOT]se

## Memory footprint and more
* Tested on an Arduino Nano v3 ATMega 328, Arduino IDE 1.60, ESP8266 module with firmware version 0.9.2.4
* approx 3.5kB of program storage
* approx 285 bytes or RAM


## Install
* Download the library as a zip from https://github.com/ekstrand/SerialESP8266wifi/archive/master.zip 
* Unzip and place in ARDUINO_HOME/libraries/ directory as SerialESP8266wifi
* Restart the Arduino IDE
* In your sketch do a `#include <SerialESP8266wifi.h>`
* To set up a simple server for testing, I like to use SocketTest http://sourceforge.net/projects/sockettest/

## Constructor

**SerialESP8266wifi(Stream serialIn, Stream serialOut, byte resetPin)**
* **serialIn** this object is used to read from the ESP8266, you can use either hardware or software serial
* **serialOut** this object is used to write to the ESP8266, you can use either hardware or software serial
* **resetPin** this pin will be pulled low then high to reset the ESP8266. It is assumed that a the CH_PD pin is connected the this pin. See pin out and more at: http://www.electrodragon.com/w/ESP8266#Module_Pin_Description
* **Example:** ```SerialESP8266wifi wifi(swSerial, swSerial, 10);```

**SerialESP8266wifi(Stream serialIn, Stream serialOut, byte resetPin, Stream debugSerial)**
* **serialIn** this object is used to read from the ESP8266, you can use either hardware or software serial
* **serialOut** this object is used to write to the ESP8266, you can use either hardware or software serial
* **resetPin** this pin will be pulled low then high to reset the ESP8266. It is assumed that a the CH_PD pin is connected the this pin. See pin out and more at: http://www.electrodragon.com/w/
ESP8266#Module_Pin_Description
* **debugSerial** enables wifi debug and local echo to Serial (could be hw or sw)
* **Example:** ```SerialESP8266wifi wifi(swSerial, swSerial, 10, Serial);```


## Starting the module
**boolean begin()** calling this method will do a hw reset on the ESP8266 and set basic parameters
* **return** will return a true or false depending if the module was properly initiated
* **Example:** `boolean esp8266started = wifi.begin();`

## Connecting to an access point
**boolean connectToAP(char * ssid, char*  password)** tells the ESP8266 to connect to an accesspoint
* **ssid** the ssid (station name) to be used. Note that this method uses char arrays as input. See http://arduino.cc/en/Reference/StringToCharArray for how to convert an arduino string object to a char array (max 15 chars)
* **password** the access point password wpa/wpa2 is assumed (max 15 chars)
* **return** will return a true if a valid IP was received within the time limit (15 seconds)
* **Example:** `boolean apConnected = wifi.connectToAP("myaccesspoint", "password123");`

**boolean isConnectedToAP()** checks if the module is connected with a valid IP
* **return** will return a true if the module has an valid IP address
* **Example:** `boolean apConnected = wifi.isConnectedToAP();`

## Connecting to a server
**boolean connectToServer(char* ip, char* port)** tells the ESP8266 to open a connection to a server
* **ip** the IP-address of the server to connect to
* **port** the port number to be used
* **return** true if connection is established within 5 seconds
* **Example:** `boolean serverConnected =  wifi.connectToServer("192.168.5.123", "2121");`

**boolean isConnectedToServer()** checks if a server is connected
* **return** will return a true if we are connected to a server
* **Example:** `boolean serverConnected = wifi.isConnectedToServer();`

**setTransportToTCP() AND setTransportToUDP()** tells the ESP8266 which transport to use when connecting to a server. Default is TCP.

## Disconnecting from a server
**disconnectFromServer()** tells the ESP8266 to close the server connection
* **Example:** `wifi.disconnectFromServer();`

## Sending a message
**boolean send(char channel, char * message)** sends a message - alias for send(char channel, char * message, true)
* **channel** Set to **SERVER** if you want to send to server. If we are the server, the value can be between '1'-'3'
* **message** a character array, max 25 characters long.
* **return** true if the message was sent
* **Example:** `boolean sendOk = wifi.send(SERVER, "Hello World!");`

**boolean send(char channel, char * message, boolean sendNow)** sends or queues a message for later sending
* **channel** Set to **SERVER** if you want to send to server. If we are the server, the value can be between '1'-'3'
* **message** a character array, max 25 characters long.
* **sendNow** if false, the message is appended to a buffer, if true the message is sent right away
* **return** true if the message was sent
* **Example:** 
```
wifi.send(SERVER, "You", false);
wifi.send(SERVER, " are ", false);
wifi.send(SERVER, "fantastic!", true); // ie wifi.send(SERVER, "fantastic!");
```
**endSendWithNewline(bool endSendWithNewline)** by default all messages are sent with newline and carrage return (println), you can disable this
* **endSendWithNewline** sent messages with print instead of println
* **Example:** `wifi.endSendWithNewline(false);`

## Checking Client Connections
**boolean checkConnections(&connections)** - Updates pre-initialised pointer to
WifiConnection \*connections.
* **return** true if client is connected
* Updated pointer is array of 3 connections:
  * **boolean connected** true if connected.
  * **char channel** channel number, can be passed to `send`.
* **Example:**
```
WifiConnection *connections;

wifi.checkConnections(&connections);
for (int i = 0; i < MAX_CONNECTIONS; i++) {
  if (connections[i].connected) {
    // See if there is a message
    WifiMessage msg = wifi.getIncomingMessage();
    // Check message is there
    if (msg.hasData) {
      processCommand(msg);
    }
  }
}
```

## Check Connection
**boolean isConnection(void)** - Returns true if client is connected,
otherwise false. Use as above without WifiConnection pointer if not
bothered about multi-client.

## Get Incoming Message From Connected Client
**WifiMessage getIncomingMessage(void)** - checks serial buffer for messages.
Return is WifiMessage type as below. See example Check Client Connection
example for usage.

## Receiving messages
**WifiMessage listenForIncomingMessage(int timeoutMillis)** will listen for new messages up to timeoutMillis milliseconds. Call this method as often as possible and with as large timeoutMillis as possible to be able to catch as many messages as possible..
* **timeoutMillis** the maximum number of milliseconds to look for a new incoming message
* **return** WifiMessage contains:
 * **boolean hasData** true if a message was received
 * **char channel** tells you if the message was received from the server (channel == SERVER) or another source
 * **char * message** the message as a character array (up to the first 25 characters)
* **Example:** 
```
void loop(){
    WifiMessage in = wifi.listenForIncomingMessage(6000);
    if (in.hasData) {
        Serial.print("Incoming message:");
        Serial.println(in.message);
        if(in.channel == SERVER)
            Serial.println("From server");
        else{
            Serial.print("From channel:");
            Serial.println(in.channel);
        }
    }
    // Do other stuff
 }
```

## Local access point and local server
**boolean startLocalAPAndServer(char* ssid, char* password, char* channel, char* port)** will create an local access point and start a local server
* **ssid** the name for your access point, max 15 characters
* **password** the password for your access point, max 15 characters
* **channel** the channel for your access point
* **port** the port for your local server, TCP only
* **return** true if the local access point and server was configured and started
* **Example:** `boolean localAPAndServerStarted = wifi.startLocalAPAndServer("my_ap", "secret_pwd", "5", "2121");`

**boolean stopLocalAPAndServer()** disable the accesspoint (the server will not be stopped, since a restart is needed)
* **return** true if the local access point was stopped
* **Example:** `boolean localAPAndServerStopped = wifi.stopLocalAPAndServer();`

**boolean isLocalAPAndServerRunning()** check if local access point and server is running
* **return** true if the local access point and local server is running
* **Example:** `boolean localAPAndServerRunning = wifi.isLocalAPAndServerRunning();`

## Re-connect functionality
Everytime send(...)  and listenForIncomingMessage(..) is called a watchdog checks that the configured access point, server and local access point and server is still running, if not they will be restarted or re-connected. The same thing happens if the ESP8266 should reset.
Note: It is really only the send method that can detect a lost connection to the server. To be sure you are connected, do a send once in a while..

## Avanced configuration
In SerialESP8266wifi.h you can change some stuff:
* **HW_RESET_RETRIES 3** - is the maximum number of times begin() will try to start the ESP8266 module
* **SERVER_CONNECT_RETRIES_BEFORE_HW_RESET 30** - is the nr of time the watchdog will try to establish connection to a server before a hardware reset of the ESP8266 is performed
* The maximum number of characters for incoming and outgoing messages can be changes by editing:
    * char msgOut[26];
    * char msgIn[26];
* If the limit for ssid and password length does not suite you, please change:
    * char _ssid[16];
    * char _password[16];
    *  char _localAPSSID[16];
    *  char _localAPPassword[16];















