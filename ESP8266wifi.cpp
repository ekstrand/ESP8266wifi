//
//  esp8266wifi.cpp
//
//
//  Created by Jonas Ekstrand on 2015-02-20.
//
//

#include "ESP8266wifi.h"

// Workaround for http://gcc.gnu.org/bugzilla/show_bug.cgi?id=34734
#ifdef PROGMEM
#undef PROGMEM
#define PROGMEM __attribute__((section(".progmem.data")))
#endif

const char OK[] PROGMEM = "OK";
const char FAIL[] PROGMEM = "FAIL";
const char ERROR[] PROGMEM = "ERROR";
const char NO_CHANGE[] PROGMEM = "no change";

const char SEND_OK[] PROGMEM = "SEND OK";
const char LINK_IS_NOT[] PROGMEM = "link is not";
const char PROMPT[] PROGMEM = ">";
const char BUSY[] PROGMEM =  "busy";
const char LINKED[] PROGMEM = "Linked";
const char ALREADY[] PROGMEM = "ALREAY";//yes typo in firmware..
const char READY[] PROGMEM = "ready";
const char NO_IP[] PROGMEM = "0.0.0.0";

const char CIPSEND[] PROGMEM = "AT+CIPSEND=";
const char CIPSERVER[] PROGMEM = "AT+CIPSERVER=1,";
const char CIPSTART[] PROGMEM = "AT+CIPSTART=4,\"";
const char TCP[] PROGMEM = "TCP";
const char UDP[] PROGMEM = "UDP";

const char CWJAP[] PROGMEM = "AT+CWJAP=\"";

const char CWMODE_1[] PROGMEM = "AT+CWMODE=1";
const char CWMODE_3[] PROGMEM = "AT+CWMODE=3";
const char CWMODE_CHECK[] PROGMEM = "AT+CWMODE?";
const char CWMODE_OK[] PROGMEM = "+CWMODE:1";

const char CIFSR[] PROGMEM = "AT+CIFSR";
const char CIPMUX_1[] PROGMEM = "AT+CIPMUX=1";

const char ATE0[] PROGMEM = "ATE0";
const char ATE1[] PROGMEM = "ATE1";

const char CWSAP[] PROGMEM = "AT+CWSAP=\"";

const char IPD[] PROGMEM = "+IPD,";

const char COMMA[] PROGMEM = ",";
const char COMMA_1[] PROGMEM = "\",";
const char COMMA_2[] PROGMEM = "\",\"";
const char THREE_COMMA[] PROGMEM = ",3";
const char DOUBLE_QUOTE[] PROGMEM = "\"";

ESP8266wifi::ESP8266wifi(Stream &serialIn, Stream &serialOut, byte resetPin, bool echo) {
    _serialIn = &serialIn;
    _serialOut = &serialOut;
    _resetPin = resetPin;
    
    pinMode(_resetPin, OUTPUT);
    digitalWrite(_resetPin, LOW);//Start with radio off
    
    flags.echoOnOff = echo;
    flags.connectToServerUsingTCP = true;
    flags.endSendWithNewline = true;
    flags.started = false;
    flags.localAPandServerConfigured = false;
    flags.apConfigured = false;
    flags.serverConfigured = false;
}

void ESP8266wifi::endSendWithNewline(bool endSendWithNewline){
    flags.endSendWithNewline = endSendWithNewline;
}

void ESP8266wifi::loadString(const char* str, char* out) {
    strcpy_P(out, (char*)str);
}

bool ESP8266wifi::begin() {
    msgOut[0] = '\0';
    msgIn[0] = '\0';
    flags.connectedToServer = false;
    flags.localAPAndServerRunning = false;
    serverRetries = 0;
    
    //Do a HW reset
    bool statusOk = false;
    byte i;
    for(i =0; i<HW_RESET_RETRIES; i++){
        loadString(NO_IP, buf);
        findString(10, buf); //Cleanup
        digitalWrite(_resetPin, LOW);
        delay(500);
        digitalWrite(_resetPin, HIGH); // select the radio
        // Look for ready string from wifi module
        loadString(READY, buf);
        statusOk = findString(3000, buf) == 1;
        if(statusOk)
            break;
    }
    if (!statusOk)
        return false;
    
    //Turn local AP = off
    loadString(CWMODE_1,buf);
    _serialOut -> println(buf);
    loadString(OK,buf);
    loadString(NO_CHANGE,buf2);
    if (findString(1000, buf, buf2) == 0)
        return false;
    
    // Set echo on/off
    if(flags.echoOnOff)//if echo = true
        loadString(ATE1,buf);
    else
        loadString(ATE0,buf);
    _serialOut -> println(buf);
    loadString(OK,buf);
    loadString(NO_CHANGE,buf2);
    if (findString(1000, buf, buf2) == 0)
        return false;
    
    // Set mux to enable multiple connections
    loadString(CIPMUX_1,buf);
    _serialOut -> println(buf);
    loadString(OK,buf);
    loadString(NO_CHANGE,buf2);
    flags.started = findString(3000, buf, buf2) > 0;
    return flags.started;
}

bool ESP8266wifi::isStarted(){
    return flags.started;
}
void ESP8266wifi::restart(){
    begin();
    
    if(flags.localAPandServerConfigured)
        startLocalAPAndServer();
    
    if(flags.apConfigured)
        connectToAP();
    
    if(flags.serverConfigured)
        connectToServer();
}

bool ESP8266wifi::connectToAP(const char* ssid, const char* password){
    strncpy(_ssid, ssid, sizeof _ssid);
    strncpy(_password, password, sizeof _password);
    flags.apConfigured = true;
    return connectToAP();
}

bool ESP8266wifi::connectToAP(){
    loadString(CWJAP,buf);
    _serialOut -> print(buf);
    
    _serialOut -> print(_ssid);
    
    loadString(COMMA_2,buf);
    _serialOut -> print(buf);
    
    _serialOut -> print(_password);
    
    loadString(DOUBLE_QUOTE,buf);
    _serialOut -> println(buf);
    
    loadString(OK,buf);
    loadString(FAIL, buf2);
    findString(15000, buf, buf2);
    return isConnectedToAP();
}

bool ESP8266wifi::isConnectedToAP(){
    loadString(CIFSR,buf);
    _serialOut -> println(buf);
    
    loadString(NO_IP,buf);
    loadString(ERROR,buf2);
    byte code = findString(350, buf, buf2);
    
    loadString(OK,buf);
    findString(10, buf); //cleanup
    return (code == 0);
}

void ESP8266wifi::setTransportToUDP(){
    flags.connectToServerUsingTCP = false;
}

void ESP8266wifi::setTransportToTCP(){
    flags.connectToServerUsingTCP = true;
}

bool ESP8266wifi::connectToServer(const char* ip, const char* port){
    strncpy(_ip, ip, sizeof _ip);
    strncpy(_port, port, sizeof _port);
    flags.serverConfigured = true;
    return connectToServer();
}

bool ESP8266wifi::connectToServer(){
    loadString(CIPSTART,buf);
    _serialOut -> print(buf);
    
    if(flags.connectToServerUsingTCP)
        loadString(TCP,buf);
    else
        loadString(UDP,buf);
    _serialOut -> print(buf);
    
    loadString(COMMA_2,buf);
    _serialOut -> print(buf);
    
    _serialOut -> print(_ip);
    
    loadString(COMMA_1,buf);
    _serialOut -> print(buf);
    
    _serialOut -> println(_port);
    
    loadString(LINKED,buf);
    loadString(ALREADY,buf2);
    
    flags.connectedToServer = (findString(5000, buf, buf2) > 0);
    
    if(flags.connectedToServer)
        serverRetries = 0;
    return flags.connectedToServer;
}

bool ESP8266wifi::isConnectedToServer(){
    if(flags.connectedToServer)
        serverRetries = 0;
    return flags.connectedToServer;
}

bool ESP8266wifi::startLocalAPAndServer(const char* ssid, const char* password, const char* channel, const char* port){
    strncpy(_localAPSSID, ssid, sizeof _localAPSSID);
    strncpy(_localAPPassword, password, sizeof _localAPPassword);
    strncpy(_localAPChannel, channel, sizeof _localAPChannel);
    strncpy(_localServerPort, port, sizeof _localServerPort);
    
    flags.localAPandServerConfigured = true;
    return startLocalAPAndServer();
}

bool ESP8266wifi::startLocalAPAndServer(){
    // Start local ap mode (eg both local ap and ap)
    loadString(CWMODE_3, buf);
    
    _serialOut -> println(buf);
    
    loadString(OK,buf);
    loadString(NO_CHANGE,buf2);
    if (!findString(2000, buf,buf2))
        return false;
    
    // Configure the soft ap
    loadString(CWSAP,buf);
    _serialOut -> print(buf);
    _serialOut -> print(_localAPSSID);
    
    loadString(COMMA_2,buf);
    _serialOut -> print(buf);
    
    _serialOut -> print(_localAPPassword);
    
    loadString(COMMA_1,buf);
    _serialOut -> print(buf);
    
    _serialOut -> print(_localAPChannel);
    
    loadString(THREE_COMMA, buf);
    _serialOut -> println(buf);
    
    loadString(OK,buf);
    loadString(ERROR,buf2);
    
    if(findString(5000, buf, buf2) != 1)
        return false;
    
    // Start local server
    loadString(CIPSERVER,buf);
    _serialOut -> print(buf);
    _serialOut -> println(_localServerPort);
    
    loadString(OK,buf);
    loadString(NO_CHANGE,buf2);
    flags.localAPAndServerRunning = (findString(2000, buf, buf2) > 0);
    return flags.localAPAndServerRunning;
}

bool ESP8266wifi::stopLocalAPAndServer(){
    //NOT STOPPING SERVER = RESTART..
    loadString(CWMODE_1, buf);
    _serialOut -> println(buf);
    
    loadString(OK, buf);
    loadString(NO_CHANGE, buf2);
    
    boolean stopped = (findString(2000, buf, buf2)>0);
    flags.localAPAndServerRunning = !stopped;
    flags.localAPandServerConfigured = false; //to prevent autostart
    return stopped;
}


bool ESP8266wifi::isLocalAPAndServerRunning(){
    return flags.localAPAndServerRunning;
}

void ESP8266wifi::watchdog(){
    //Give up do a hw reset
    if(serverRetries >= SERVER_CONNECT_RETRIES_BEFORE_HW_RESET){
        restart();
    }
    
    //Try to start server and or ap if needed
    if(flags.serverConfigured && !isConnectedToServer()){
        connectToServer();
        serverRetries++;
        //If still not connected!? try to reconnect to the ap
        if(!isConnectedToServer() && !isConnectedToAP() && flags.apConfigured){
            boolean apOk  = connectToAP();
            if(!apOk)//If not ok yet, try again..
                delay(2000);
            //If connection to AP failes, try a chip reset
            if(!apOk && !isConnectedToAP()){
                restart();
            }else{//Success we managed to connect to ap! Try server again
                connectToServer();
            }
        }
    }
}

/*
 * Will always send with a linefeed at the end..
 */
bool ESP8266wifi::send(char channel, const char * message, bool sendNow){
    watchdog();
    byte totalChars = strlen(message) +  strlen(msgOut) + 1; // add one to account for null char...
    strcat(msgOut, message);
    msgOut[strlen(msgOut)] = '\0';
    if (!sendNow)
        return true;
    byte length = strlen(msgOut);
    
    if(flags.endSendWithNewline)
        length += 2;
    
    loadString(CIPSEND,buf);
    _serialOut -> print(buf);
    _serialOut -> print(channel);
    loadString(COMMA,buf);
    _serialOut -> print(buf);
    _serialOut -> println(length);
    loadString(PROMPT,buf);
    loadString(LINK_IS_NOT,buf2);
    byte prompt = findString(1000, buf, buf2);
    if (prompt != 2) {
        if(flags.endSendWithNewline)
            _serialOut -> println(msgOut);
        else
            _serialOut -> print(msgOut);
        loadString(SEND_OK,buf);
        loadString(BUSY, buf2);
        byte sendStatus = findString(5000, buf, buf2);
        if (sendStatus == 1) {
            msgOut[0] = '\0';
            if(channel == SERVER)
                flags.connectedToServer = true;
            return true;
        }
    }
    //else
    if(channel == SERVER)
        flags.connectedToServer = false;
    msgOut[0] = '\0';
    return false;
}

bool ESP8266wifi::send(char channel, const char * message){
    return send(channel,message,true);
}

WifiMessage ESP8266wifi::listenForIncomingMessage(int timeout){
    watchdog();
    msgIn[0] = '\0';
    
    static WifiMessage msg;
    
    msg.hasData = false;
    msg.channel = '-';
    msg.message = msgIn;
    
    loadString(IPD,buf);
    loadString(READY,buf2);
    
    //TODO listen for unlink etc...
    byte msgOrRestart = findString(timeout, buf, buf2);
    buf[0] = '\0';
    
    //Detected a esp8266 restart
    if (msgOrRestart == 2){
        restart();
        return msg;
    }
    //Message received..
    else if (msgOrRestart == 1) {
        char channel = _serialIn -> read();
        if(channel == SERVER)
            flags.connectedToServer = true;
        _serialIn -> read(); // removing commma
        byte length = 0;
        while (_serialIn -> available()) {
            char c = _serialIn -> read();
            if (c == ':')
                break;
            else{
                buf[length++] = (char)c;
            }
        }
        //Extract the number of chars...
        buf[length] = '\0';
        length = atoi(buf);
        byte i;
        for (i = 0; i < length; i++) {
            if( ((sizeof msgIn) - 2) >= i) // Only read until buffer is full - null char
                msgIn[i] =  (char) _serialIn -> read();
            else
                break;
        }
        msgIn[i] = '\0'; //terminate string
        msg.hasData = true;
        msg.channel = channel;
        msg.message = msgIn;
        loadString(OK, buf);
        findString(10, buf); // cleanup after rx
    }
    return msg;
}

void  ESP8266wifi::debug(Stream &dbgSerial){
    _dbgSerial = &dbgSerial;
    flags.debug = true;
}

byte ESP8266wifi::findString(int timeout,  const char * what){
    return findString(timeout, what, "");
}

byte ESP8266wifi::findString(int timeout,  const char * what,  const char * what2) {
    unsigned long stop = millis() + timeout;
    byte nrChars1 = strlen(what);
    byte nextPosMatch1 = 0;
    byte nrChars2 = -1;
    if (strlen(what2) > 0)
        nrChars2 = strlen(what2);
    byte nextPosMatch2 = 0;
    char c;
    while ((millis() < stop)) {
        while (_serialIn -> available()) {
            c = (_serialIn -> read());
            if(flags.debug)
                _dbgSerial -> print(c);
            if (c == what[nextPosMatch1])
                ++nextPosMatch1;
            else
                nextPosMatch1 = 0;
            if (strlen(what2) > 0 && c == what2[nextPosMatch2])
                ++nextPosMatch2;
            else
                nextPosMatch2 = 0;
            if (nextPosMatch1 == nrChars1) {
                return 1;
            }
            if (nextPosMatch2 == nrChars2) {
                return 2;
            }
        }
        delay(10);
    }
    return 0;
}
