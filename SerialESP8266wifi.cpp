//
//  Serialesp8266wifi.cpp
//
//
//  Created by Jonas Ekstrand on 2015-02-20.
//
//

#include "SerialESP8266wifi.h"
#include "Arduino.h"

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
const char CIPSERVERSTART[] PROGMEM = "AT+CIPSERVER=1,";
const char CIPSERVERSTOP[] PROGMEM = "AT+CIPSERVER=0";
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

const char IPD[] PROGMEM = "IPD,";
const char CONNECT[] PROGMEM = "CONNECT";
const char CLOSED[] PROGMEM = "CLOSED";

const char COMMA[] PROGMEM = ",";
const char COMMA_1[] PROGMEM = "\",";
const char COMMA_2[] PROGMEM = "\",\"";
const char THREE_COMMA[] PROGMEM = ",3";
const char DOUBLE_QUOTE[] PROGMEM = "\"";
const char EOL[] PROGMEM = "\n";

const char STAIP[] PROGMEM = "STAIP,\"";
const char STAMAC[] PROGMEM = "STAMAC,\"";

SerialESP8266wifi::SerialESP8266wifi(Stream &serialIn, Stream &serialOut, byte resetPin) {
    _serialIn = &serialIn;
    _serialOut = &serialOut;
    _resetPin = resetPin;

    pinMode(_resetPin, OUTPUT);
    digitalWrite(_resetPin, LOW);//Start with radio off

    flags.connectToServerUsingTCP = true;
    flags.endSendWithNewline = true;
    flags.started = false;
    flags.localServerConfigured = false;
    flags.localApConfigured = false;
    flags.apConfigured = false;
    flags.serverConfigured = false;

    flags.debug = false;
    flags.echoOnOff = false;

    for (int i = 0; i < MAX_CONNECTIONS; i++) {
      _connections[i].channel = i + 0x30; // index to ASCII
      _connections[i].connected = false;
    }
}

SerialESP8266wifi::SerialESP8266wifi(Stream &serialIn, Stream &serialOut, byte resetPin, Stream &dbgSerial) {
    _serialIn = &serialIn;
    _serialOut = &serialOut;
    _resetPin = resetPin;

    pinMode(_resetPin, OUTPUT);
    digitalWrite(_resetPin, LOW);//Start with radio off

    flags.connectToServerUsingTCP = true;
    flags.endSendWithNewline = true;
    flags.started = false;
    flags.localServerConfigured = false;
    flags.localApConfigured = false;
    flags.apConfigured = false;
    flags.serverConfigured = false;

    _dbgSerial = &dbgSerial;
    flags.debug = true;
    flags.echoOnOff = true;

    for (int i = 0; i < MAX_CONNECTIONS; i++) {
      _connections[i].channel = i + 0x30; // index to ASCII
      _connections[i].connected = false;
    }
}

void niceDelay(int duration){
    unsigned long startMillis = millis();
    while(millis() - startMillis < duration){
        sqrt(4700);
    }
}

void SerialESP8266wifi::endSendWithNewline(bool endSendWithNewline){
    flags.endSendWithNewline = endSendWithNewline;
}

bool SerialESP8266wifi::begin() {
    msgOut[0] = '\0';
    msgIn[0] = '\0';
    flags.connectedToServer = false;
    flags.localServerConfigured = false;
    flags.localApConfigured = false;
    serverRetries = 0;

    //Do a HW reset
    bool statusOk = false;
    byte i;
    for(i =0; i<HW_RESET_RETRIES; i++){
        readCommand(10, NO_IP); //Cleanup
        digitalWrite(_resetPin, LOW);
        niceDelay(500);
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

bool SerialESP8266wifi::isStarted(){
    return flags.started;
}

bool SerialESP8266wifi::restart() {
    return begin()
        && (!flags.localApConfigured || startLocalAp())
        && (!flags.localServerConfigured || startLocalServer())
        && (!flags.apConfigured || connectToAP())
        && (!flags.serverConfigured || connectToServer());
}

bool SerialESP8266wifi::connectToAP(String& ssid, String& password) {
    return connectToAP(ssid.c_str(), password.c_str());
}

bool SerialESP8266wifi::connectToAP(const char* ssid, const char* password){//TODO make timeout config or parameter??
    strncpy(_ssid, ssid, sizeof _ssid);
    strncpy(_password, password, sizeof _password);
    flags.apConfigured = true;
    return connectToAP();
}

bool SerialESP8266wifi::connectToAP(){
    writeCommand(CWJAP);
    _serialOut -> print(_ssid);
    writeCommand(COMMA_2);
    _serialOut -> print(_password);
    writeCommand(DOUBLE_QUOTE, EOL);

    readCommand(15000, OK, FAIL);
    return isConnectedToAP();
}

bool SerialESP8266wifi::isConnectedToAP(){
    writeCommand(CIFSR, EOL);
    byte code = readCommand(350, NO_IP, ERROR);
    readCommand(10, OK); //cleanup
    return (code == 0);
}

char* SerialESP8266wifi::getIP(){
    msgIn[0] = '\0';
    writeCommand(CIFSR, EOL);
    byte code = readCommand(1000, STAIP, ERROR);
    if (code == 1) {
        // found staip
        readBuffer(&msgIn[0], sizeof(msgIn) - 1, '"');
        readCommand(10, OK, ERROR);
        return &msgIn[0];
    }
    readCommand(1000, OK, ERROR);
    return &msgIn[0];
}

char* SerialESP8266wifi::getMAC(){
    msgIn[0] = '\0';
    writeCommand(CIFSR, EOL);
    byte code = readCommand(1000, STAMAC, ERROR);
    if (code == 1) {
        // found stamac
        readBuffer(&msgIn[0], sizeof(msgIn) - 1, '"');
        readCommand(10, OK, ERROR);
        return &msgIn[0];
    }
    readCommand(1000, OK, ERROR);
    return &msgIn[0];
}

void SerialESP8266wifi::setTransportToUDP(){
    flags.connectToServerUsingTCP = false;
}

void SerialESP8266wifi::setTransportToTCP(){
    flags.connectToServerUsingTCP = true;
}

bool SerialESP8266wifi::connectToServer(String& ip, String& port) {
    return connectToServer(ip.c_str(), port.c_str());
}

bool SerialESP8266wifi::connectToServer(const char* ip, const char* port){//TODO make timeout config or parameter??
    strncpy(_ip, ip, sizeof _ip);
    strncpy(_port, port, sizeof _port);
    flags.serverConfigured = true;
    return connectToServer();
}


bool SerialESP8266wifi::connectToServer(){
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


void SerialESP8266wifi::disconnectFromServer(){
    flags.connectedToServer = false;
    flags.serverConfigured = false;//disable reconnect
    writeCommand(CIPCLOSE);
    readCommand(2000, OK); //fire and forget in this case..
}


bool SerialESP8266wifi::isConnectedToServer(){
    if(flags.connectedToServer)
        serverRetries = 0;
    return flags.connectedToServer;
}

bool SerialESP8266wifi::startLocalAPAndServer(const char* ssid, const char* password, const char* channel, const char* port){
    strncpy(_localAPSSID, ssid, sizeof _localAPSSID);
    strncpy(_localAPPassword, password, sizeof _localAPPassword);
    strncpy(_localAPChannel, channel, sizeof _localAPChannel);
    strncpy(_localServerPort, port, sizeof _localServerPort);

    flags.localApConfigured = true;
    flags.localServerConfigured = true;
    return startLocalAp() && startLocalServer();
}

bool SerialESP8266wifi::startLocalAP(const char* ssid, const char* password, const char* channel){
    strncpy(_localAPSSID, ssid, sizeof _localAPSSID);
    strncpy(_localAPPassword, password, sizeof _localAPPassword);
    strncpy(_localAPChannel, channel, sizeof _localAPChannel);

    flags.localApConfigured = true;
    return startLocalAp();
}

bool SerialESP8266wifi::startLocalServer(const char* port) {
    strncpy(_localServerPort, port, sizeof _localServerPort);
    flags.localServerConfigured = true;
    return startLocalServer();
}

bool SerialESP8266wifi::startLocalServer(){
    // Start local server
    writeCommand(CIPSERVERSTART);
    _serialOut -> println(_localServerPort);

    flags.localServerRunning = (readCommand(2000, OK, NO_CHANGE) > 0);
    return flags.localServerRunning;
}

bool SerialESP8266wifi::startLocalAp(){
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

    flags.localApRunning = (readCommand(5000, OK, ERROR) == 1);
    return flags.localApRunning;
}

bool SerialESP8266wifi::stopLocalServer(){
    writeCommand(CIPSERVERSTOP, EOL);
    boolean stopped = (readCommand(2000, OK, NO_CHANGE) > 0);
    flags.localServerRunning = !stopped;
    flags.localServerConfigured = false; //to prevent autostart
    return stopped;
}

bool SerialESP8266wifi::stopLocalAP(){
    writeCommand(CWMODE_1, EOL);

    boolean stopped = (readCommand(2000, OK, NO_CHANGE) > 0);
    flags.localApRunning = !stopped;
    flags.localApConfigured = false; //to prevent autostart
    return stopped;
}

bool SerialESP8266wifi::stopLocalAPAndServer(){
    return stopLocalAP() && stopLocalServer();
}

bool SerialESP8266wifi::isLocalAPAndServerRunning(){
    return flags.localApRunning & flags.localServerRunning;
}

// Performs a connect retry (or hardware reset) if not connected
bool SerialESP8266wifi::watchdog() {
    if (serverRetries >= SERVER_CONNECT_RETRIES_BEFORE_HW_RESET) {
        // give up, do a hardware reset
        return restart();
    }
    if (flags.serverConfigured && !flags.connectedToServer) {
        serverRetries++;
        if (flags.apConfigured && !isConnectedToAP()) {
            if (!connectToAP()) {
                // wait a bit longer, then check again
                niceDelay(2000);
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
bool SerialESP8266wifi::send(char channel, String& message, bool sendNow) {
    return send(channel, message.c_str(), sendNow);
}

bool SerialESP8266wifi::send(char channel, const char * message, bool sendNow){
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
    else
        _connections[channel-0x30].connected = false;
    msgOut[0] = '\0';
    return false;
}

// Checks to see if there is a client connection
bool SerialESP8266wifi::isConnection(void) {
    WifiConnection *connections;

    // return the first channel, assume single connection use
    return checkConnections(&connections);
}

// Updates private connections struct and make passed pointer point to data
bool SerialESP8266wifi::checkConnections(WifiConnection **pConnections) {
    watchdog();
    // setup buffers on stack & copy data from PROGMEM pointers
    char buf1[16] = {'\0'};
    char buf2[16] = {'\0'};
    char buf3[16] = {'\0'};
    strcpy_P(buf1, CONNECT);
    strcpy_P(buf2, READY);
    strcpy_P(buf3, CLOSED);
    byte len1 = strlen(buf1);
    byte len2 = strlen(buf2);
    byte len3 = strlen(buf3);
    byte pos = 0;
    byte pos1 = 0;
    byte pos2 = 0;
    byte pos3 = 0;
    byte ret = 0;
    char ch = '-';

    // unload buffer and check match
    while (_serialIn->available()) {
        char c = readChar();
        // skip white space
        if (c != ' ') {
          // get out of here if theres a message
          if (c == '+')
            break;
          // first char is channel
          if (pos == 0)
            ch = c;
          pos++;
          pos1 = (c == buf1[pos1]) ? pos1 + 1 : 0;
          pos2 = (c == buf2[pos2]) ? pos2 + 1 : 0;
          pos3 = (c == buf3[pos3]) ? pos3 + 1 : 0;
          if (len1 > 0 && pos1 == len1) {
              ret = 1;
              break;
          }
          if (len2 > 0 && pos2 == len2) {
              ret = 2;
              break;
          }
          if (len3 > 0 && pos3 == len3) {
              ret = 3;
              break;
          }
        }
    }

    if (ret == 2)
      restart();

    // new connection
    if (ret == 1) {
      _connections[ch-0x30].connected = true;
      *pConnections = _connections;
      if (ch == SERVER)
          flags.connectedToServer = true;
      return 1;
    }

    // channel disconnected
    if (ret == 3) {
      _connections[ch-0x30].connected = false;
      *pConnections = _connections;
      if (ch == SERVER)
          flags.connectedToServer = false;
      return 0;
    }

    // nothing has changed return single connection status
    *pConnections = _connections;
    return _connections[0].connected;
}

WifiMessage SerialESP8266wifi::listenForIncomingMessage(int timeout){
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
        char channel = readChar();
        if (channel == SERVER)
            flags.connectedToServer = true;
        readChar(); // removing comma
        readBuffer(&buf[0], sizeof(buf) - 1, ':'); // read char count
        readChar(); // removing ':' delim
        byte length = atoi(buf);
        readBuffer(&msgIn[0], min(length, sizeof(msgIn) - 1));
        msg.hasData = true;
        msg.channel = channel;
        msg.message = msgIn;
        readCommand(10, OK); // cleanup after rx
    }
    return msg;
}

WifiMessage SerialESP8266wifi::getIncomingMessage(void) {
    watchdog();
    char buf[16] = {'\0'};
    msgIn[0] = '\0';

    static WifiMessage msg;

    msg.hasData = false;
    msg.channel = '-';
    msg.message = msgIn;

    // See if a message has come in (block 1s otherwise misses?)
    byte msgOrRestart = readCommand(10, IPD, READY);

    //Detected a esp8266 restart
    if (msgOrRestart == 2){
        restart();
        return msg;
    }
    //Message received..
    else if (msgOrRestart == 1) {
        char channel = readChar();
        if (channel == SERVER)
            flags.connectedToServer = true;
        readChar(); // removing comma
        readBuffer(&buf[0], sizeof(buf) - 1, ':'); // read char count
        readChar(); // removing ':' delim
        byte length = atoi(buf);
        readBuffer(&msgIn[0], min(length, sizeof(msgIn) - 1));
        msg.hasData = true;
        msg.channel = channel;
        msg.message = msgIn;
        readCommand(10, OK); // cleanup after rx
    }
    return msg;
}

// Writes commands (from PROGMEM) to serial output
void SerialESP8266wifi::writeCommand(const char* text1 = NULL, const char* text2) {
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
byte SerialESP8266wifi::readCommand(int timeout, const char* text1, const char* text2) {
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
    do {
        while (_serialIn->available()) {
            char c = readChar();
            pos1 = (c == buf1[pos1]) ? pos1 + 1 : 0;
            pos2 = (c == buf2[pos2]) ? pos2 + 1 : 0;
            if (len1 > 0 && pos1 == len1)
                return 1;
            if (len2 > 0 && pos2 == len2)
                return 2;
        }
        niceDelay(10);
    } while (millis() < stop);
    return 0;
}

// Unload buffer without delay
/*byte ESP8266wifiESP8266wifi::readCommand(const char* text1, const char* text2) {
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
    while (_serialIn->available()) {
        char c = readChar();
        pos1 = (c == buf1[pos1]) ? pos1 + 1 : 0;
        pos2 = (c == buf2[pos2]) ? pos2 + 1 : 0;
        if (len1 > 0 && pos1 == len1)
            return 1;
        if (len2 > 0 && pos2 == len2)
            return 2;
    }
    return 0;
}*/

// Reads count chars to a buffer, or until delim char is found
byte SerialESP8266wifi::readBuffer(char* buf, byte count, char delim) {
    byte pos = 0;
    char c;
    while (_serialIn->available() && pos < count) {
        c = readChar();
        if (c == delim)
            break;
        buf[pos++] = c;
    }
    buf[pos] = '\0';
    return pos;
}

// Reads a single char from serial input (with debug printout if configured)
char SerialESP8266wifi::readChar() {
    char c = _serialIn->read();
    if (flags.debug)
        _dbgSerial->print(c);
    else
        sqrt(12345);//delayMicroseconds(50); // don't know why
    return c;
}
