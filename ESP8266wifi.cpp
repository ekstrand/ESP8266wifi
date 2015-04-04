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
const char CIPCLOSE[] PROGMEM = "AT+CIPCLOSE=4";
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
const char EOL[] PROGMEM = "\n";

ESP8266wifi::ESP8266wifi(Stream &serialIn, Stream &serialOut, byte resetPin) {
    _serialIn = &serialIn;
    _serialOut = &serialOut;
    _resetPin = resetPin;
    
    pinMode(_resetPin, OUTPUT);
    digitalWrite(_resetPin, LOW);//Start with radio off
    
    flags.connectToServerUsingTCP = true;
    flags.endSendWithNewline = true;
    flags.started = false;
    flags.localAPandServerConfigured = false;
    flags.apConfigured = false;
    flags.serverConfigured = false;
    
    flags.debug = false;
    flags.echoOnOff = false;
}


ESP8266wifi::ESP8266wifi(Stream &serialIn, Stream &serialOut, byte resetPin, Stream &dbgSerial) {
    _serialIn = &serialIn;
    _serialOut = &serialOut;
    _resetPin = resetPin;
    
    pinMode(_resetPin, OUTPUT);
    digitalWrite(_resetPin, LOW);//Start with radio off
    
    flags.connectToServerUsingTCP = true;
    flags.endSendWithNewline = true;
    flags.started = false;
    flags.localAPandServerConfigured = false;
    flags.apConfigured = false;
    flags.serverConfigured = false;
    
    _dbgSerial = &dbgSerial;
    flags.debug = true;
    flags.echoOnOff = true;    
}


void ESP8266wifi::endSendWithNewline(bool endSendWithNewline){
    flags.endSendWithNewline = endSendWithNewline;
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
        readCommand(10, NO_IP); //Cleanup
        digitalWrite(_resetPin, LOW);
        delay(500);
        digitalWrite(_resetPin, HIGH); // select the radio
        // Look for ready string from wifi module
        statusOk = readCommand(3000, READY) == 1;
        if(statusOk)
            break;
    }
    if (!statusOk)
        return false;
    
    //Turn local AP = off
    writeCommand(CWMODE_1, EOL);
    if (readCommand(1000, OK, NO_CHANGE) == 0)
        return false;
    
    // Set echo on/off
    if(flags.echoOnOff)//if echo = true
        writeCommand(ATE1, EOL);
    else
        writeCommand(ATE0, EOL);
    if (readCommand(1000, OK, NO_CHANGE) == 0)
        return false;
    
    // Set mux to enable multiple connections
    writeCommand(CIPMUX_1, EOL);
    flags.started = readCommand(3000, OK, NO_CHANGE) > 0;
    return flags.started;
}

bool ESP8266wifi::isStarted(){
    return flags.started;
}

bool ESP8266wifi::restart() {
    return begin()
        && (!flags.localAPandServerConfigured || startLocalAPAndServer())
        && (!flags.apConfigured || connectToAP())
        && (!flags.serverConfigured || connectToServer());
}

bool ESP8266wifi::connectToAP(String& ssid, String& password) {
    return connectToAP(ssid.c_str(), password.c_str());
}

bool ESP8266wifi::connectToAP(const char* ssid, const char* password){//TODO make timeout config or parameter??
    strncpy(_ssid, ssid, sizeof _ssid);
    strncpy(_password, password, sizeof _password);
    flags.apConfigured = true;
    return connectToAP();
}

bool ESP8266wifi::connectToAP(){
    writeCommand(CWJAP);
    _serialOut -> print(_ssid);
    writeCommand(COMMA_2);
    _serialOut -> print(_password);
    writeCommand(DOUBLE_QUOTE, EOL);

    readCommand(15000, OK, FAIL);
    return isConnectedToAP();
}

bool ESP8266wifi::isConnectedToAP(){
    writeCommand(CIFSR, EOL);
    byte code = readCommand(350, NO_IP, ERROR);
    readCommand(10, OK); //cleanup
    return (code == 0);
}

void ESP8266wifi::setTransportToUDP(){
    flags.connectToServerUsingTCP = false;
}

void ESP8266wifi::setTransportToTCP(){
    flags.connectToServerUsingTCP = true;
}

bool ESP8266wifi::connectToServer(String& ip, String& port) {
    return connectToServer(ip.c_str(), port.c_str());
}

bool ESP8266wifi::connectToServer(const char* ip, const char* port){//TODO make timeout config or parameter??
    strncpy(_ip, ip, sizeof _ip);
    strncpy(_port, port, sizeof _port);
    flags.serverConfigured = true;
    return connectToServer();
}


bool ESP8266wifi::connectToServer(){
    writeCommand(CIPSTART);
    if (flags.connectToServerUsingTCP)
        writeCommand(TCP);
    else
        writeCommand(UDP);
    writeCommand(COMMA_2);
    _serialOut -> print(_ip);
    writeCommand(COMMA_1);
    _serialOut -> println(_port);
    
    flags.connectedToServer = (readCommand(10000, LINKED, ALREADY) > 0);
    
    if(flags.connectedToServer)
        serverRetries = 0;
    return flags.connectedToServer;
}


void ESP8266wifi::disconnectFromServer(){
    flags.connectedToServer = false;
    flags.serverConfigured = false;//disable reconnect
    writeCommand(CIPCLOSE);
    readCommand(2000, OK); //fire and forget in this case..
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
    writeCommand(CWMODE_3, EOL);
    if (!readCommand(2000, OK, NO_CHANGE))
        return false;
    
    // Configure the soft ap
    writeCommand(CWSAP);
    _serialOut -> print(_localAPSSID);
    writeCommand(COMMA_2);
    _serialOut -> print(_localAPPassword);
    writeCommand(COMMA_1);
    _serialOut -> print(_localAPChannel);
    writeCommand(THREE_COMMA, EOL);
    
    if(readCommand(5000, OK, ERROR) != 1)
        return false;
    
    // Start local server
    writeCommand(CIPSERVER);
    _serialOut -> println(_localServerPort);
    
    flags.localAPAndServerRunning = (readCommand(2000, OK, NO_CHANGE) > 0);
    return flags.localAPAndServerRunning;
}

bool ESP8266wifi::stopLocalAPAndServer(){
    //NOT STOPPING SERVER = RESTART..
    writeCommand(CWMODE_1, EOL);

    boolean stopped = (readCommand(2000, OK, NO_CHANGE) > 0);
    flags.localAPAndServerRunning = !stopped;
    flags.localAPandServerConfigured = false; //to prevent autostart
    return stopped;
}


bool ESP8266wifi::isLocalAPAndServerRunning(){
    return flags.localAPAndServerRunning;
}

// Performs a connect retry (or hardware reset) if not connected
bool ESP8266wifi::watchdog() {
    if (serverRetries >= SERVER_CONNECT_RETRIES_BEFORE_HW_RESET) {
        // give up, do a hardware reset
        return restart();
    }
    if (flags.serverConfigured && !flags.connectedToServer) {
        serverRetries++;
        if (flags.apConfigured && !isConnectedToAP()) {
            if (!connectToAP()) {
                // wait a bit longer, then check again
                delay(2000);
                if (!isConnectedToAP()) {
                    return restart();
                }
            }
        }
        return connectToServer();
    }
    return true;
}

/*
 * Send string (if channel is connected of course)
 */
bool ESP8266wifi::send(char channel, String& message, bool sendNow) {
    return send(channel, message.c_str(), sendNow);
}

bool ESP8266wifi::send(char channel, const char * message, bool sendNow){
    watchdog();
    byte avail = sizeof(msgOut) - strlen(msgOut) - 1;
    strncat(msgOut, message, avail);
    if (!sendNow)
        return true;
    byte length = strlen(msgOut);
    
    if(flags.endSendWithNewline)
        length += 2;
    
    writeCommand(CIPSEND);
    _serialOut -> print(channel);
    writeCommand(COMMA);
    _serialOut -> println(length);
    byte prompt = readCommand(1000, PROMPT, LINK_IS_NOT);
    if (prompt != 2) {
        if(flags.endSendWithNewline)
            _serialOut -> println(msgOut);
        else
            _serialOut -> print(msgOut);
        byte sendStatus = readCommand(5000, SEND_OK, BUSY);
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

WifiMessage ESP8266wifi::listenForIncomingMessage(int timeout){
    watchdog();
    char buf[16] = {'\0'};
    msgIn[0] = '\0';
    
    static WifiMessage msg;
    
    msg.hasData = false;
    msg.channel = '-';
    msg.message = msgIn;

    //TODO listen for unlink etc...
    byte msgOrRestart = readCommand(timeout, IPD, READY);
    
    //Detected a esp8266 restart
    if (msgOrRestart == 2){
        restart();
        return msg;
    }
    //Message received..
    else if (msgOrRestart == 1) {
        char channel = _serialIn -> read();
        delayMicroseconds(50);//I dont know why ./
        if(channel == SERVER)
            flags.connectedToServer = true;
        _serialIn -> read(); // removing commma
        byte length = 0;
        while (_serialIn -> available()) {
            char c = _serialIn -> read();
            delayMicroseconds(50);//I dont know why ./
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
            if( ((sizeof msgIn) - 2) >= i) {// Only read until buffer is full - null char
                msgIn[i] =  (char) _serialIn -> read();
                if(flags.debug)
                    _dbgSerial -> print(msgIn[i]);
                else
                    delayMicroseconds(50);//I dont know why ./
            }
            else
                break;
        }
        msgIn[i] = '\0'; //terminate string
        msg.hasData = true;
        msg.channel = channel;
        msg.message = msgIn;
        readCommand(10, OK); // cleanup after rx
    }
    return msg;
}

// Writes commands (from PROGMEM) to serial output
void ESP8266wifi::writeCommand(const char* text1 = NULL, const char* text2) {
    char buf[16] = {'\0'};
    strcpy_P(buf, (char *) text1);
    _serialOut->print(buf);
    if (text2 == EOL) {
        _serialOut->println();
    } else if (text2 != NULL) {
        strcpy_P(buf, (char *) text2);
        _serialOut->print(buf);
    }
}

// Reads from serial input until a expected string is found (or until timeout)
// NOTE: strings are stored in PROGMEM (auto-copied by this method)
byte ESP8266wifi::readCommand(int timeout, const char* text1, const char* text2) {
    // setup buffers on stack & copy data from PROGMEM pointers
    char buf1[16] = {'\0'};
    char buf2[16] = {'\0'};
    if (text1 != NULL)
        strcpy_P(buf1, (char *) text1);
    if (text2 != NULL)
        strcpy_P(buf2, (char *) text2);
    byte len1 = strlen(buf1);
    byte len2 = strlen(buf2);
    byte pos1 = 0;
    byte pos2 = 0;

    // read chars until first match or timeout
    unsigned long stop = millis() + timeout;
    char c;
    do {
        while (_serialIn->available()) {
            c = _serialIn->read();
            if (flags.debug)
                _dbgSerial -> print(c);
            else
                delayMicroseconds(50);//I dont know why ./
            
            pos1 = (c == buf1[pos1]) ? pos1 + 1 : 0;
            pos2 = (c == buf2[pos2]) ? pos2 + 1 : 0;
            if (len1 > 0 && pos1 == len1)
                return 1;
            if (len2 > 0 && pos2 == len2)
                return 2;
        }
        delay(10);
    } while (millis() < stop);
    return 0;
}