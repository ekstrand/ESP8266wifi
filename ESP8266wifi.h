//
//  ESP8266wifi.h
//
//
//  Created by Jonas Ekstrand on 2015-02-20.
//  ESP8266 AT cmd ref from https://github.com/espressif/esp8266_at/wiki/CIPSERVER
//
//

#ifndef ESP8266wifi_h
#define ESP8266wifi_h

#define HW_RESET_RETRIES 3
#define SERVER_CONNECT_RETRIES_BEFORE_HW_RESET 30

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <inttypes.h>
#include <avr/pgmspace.h>
#include "HardwareSerial.h"

#define SERVER '4'


struct WifiMessage{
public:
    bool hasData:1;
    char channel;
    char * message;
};

struct Flags   // 1 byte value (on a system where 8 bits is a byte
{
    bool started:1, echoOnOff:1, debug:1, serverConfigured:1, connectedToServer:1, apConfigured:1, localAPandServerConfigured:1, localAPAndServerRunning:1, endSendWithNewline:1, connectToServerUsingTCP:1;
};

class ESP8266wifi
{
    
public:
    /*
     * Will pull resetPin low then high to reset esp8266, connect this pin to CHPD pin
     */
    ESP8266wifi(Stream &serialIn, Stream &serialOut, byte resetPin);
    
    
    /*
     * Will pull resetPin low then high to reset esp8266, connect this pin to CHPD pin
     */
    ESP8266wifi(Stream &serialIn, Stream &serialOut, byte resetPin, Stream &dbgSerial);
    
    /*
     * Will do hw reset and set inital configuration, will try this HW_RESET_RETRIES times.
     */
    bool begin(); // reset and set echo and other stuff
    
    bool isStarted();
    
    /*
     * Connect to AP using wpa encryption
     * (reconnect logic is applied, if conn lost or not established, or esp8266 restarted)
     */
    bool connectToAP(String& ssid, String& password);
    bool connectToAP(const char* ssid, const char* password);
    bool isConnectedToAP();
    
    /*
     * Connecting with TCP to server
     * (reconnect logic is applied, if conn lost or not established, or esp8266 restarted)
     */
    
    void setTransportToUDP();
    //Default..
    void setTransportToTCP();
    bool connectToServer(String& ip, String& port);
    bool connectToServer(const char* ip, const char* port);
    void disconnectFromServer();
    bool isConnectedToServer();
    
    /*
     * Starting local AP and local TCP-server
     * (reconnect logic is applied, if conn lost or not established, or esp8266 restarted)
     */
    bool startLocalAPAndServer(const char* ssid, const char* password, const char* channel,const char* port);
    bool stopLocalAPAndServer();
    bool isLocalAPAndServerRunning();
    
    
    /*
     * Send string (if channel is connected of course)
     */
    bool send(char channel, String& message, bool sendNow = true);
    bool send(char channel, const char * message, bool sendNow = true);
    
    /*
     * Default is true.
     */
    void endSendWithNewline(bool endSendWithNewline);
    
    /*
     * Scan for incoming message, do this as often and as long as you can (use as sleep in loop)
     */
    WifiMessage listenForIncomingMessage(int timeoutMillis);
    
private:
    Stream* _serialIn;
    Stream* _serialOut;
    byte _resetPin;
    
    Flags flags;
    
    bool connectToServer();
    char _ip[16];
    char _port[6];
    
    bool connectToAP();
    char _ssid[16];
    char _password[16];
    
    bool startLocalAPAndServer();
    char _localAPSSID[16];
    char _localAPPassword[16];
    char _localAPChannel[3];
    char _localServerPort[6];
    
    bool restart();
    
    byte serverRetries;
    bool watchdog();
    
    char msgOut[26];//buffer for send method
    char msgIn[26]; //buffer for listen method = limit of incoming message..

    void writeCommand(const char* text1, const char* text2 = NULL);
    byte readCommand(int timeout, const char* text1 = NULL, const char* text2 = NULL);

    Stream* _dbgSerial;
};

#endif