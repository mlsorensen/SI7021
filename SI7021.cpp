/*
Copyright 2014 Marcus Sorensen <marcus@electron14.com>

This program is licensed under the GNU LGPL v2.1
*/
#include "Arduino.h"
#include "SI7021.h"
#include <Wire.h>

#define I2C_ADDR 0x40

// I2C commands
byte RH_READ[]           = { 0xE5 };
byte TEMP_READ[]         = { 0xE3 };
byte POST_RH_TEMP_READ[] = { 0xE0 };
byte RESET[]             = { 0xFE };
byte USER1_READ[]        = { 0xE7 };
byte USER1_WRITE[]       = { 0xE6 };
byte SERIAL1_READ[]      = { 0xFA, 0x0F };
byte SERIAL2_READ[]      = { 0xFC, 0xC9 };

bool _si_exists = false;

SI7021::SI7021() {
}

bool SI7021::begin() {
    Wire.begin();
    Wire.beginTransmission(I2C_ADDR);
    if (Wire.endTransmission() == 0) {
        _si_exists = true;
    }
    return _si_exists;
}

bool SI7021::sensorExists() {
    return _si_exists;
}

int SI7021::getFahrenheitHundredths() {
    unsigned int c = getCelsiusHundredths();
    return (1.8 * c) + 3200;
}

int SI7021::getCelsiusHundredths() {
    byte tempbytes[2];
    _command(TEMP_READ, tempbytes);
    long tempraw = (long)tempbytes[0] << 8 | tempbytes[1];
    return ((17572 * tempraw) >> 16) - 4685;
}

int SI7021::_getCelsiusPostHumidity() {
    byte tempbytes[2];
    _command(POST_RH_TEMP_READ, tempbytes);
    long tempraw = (long)tempbytes[0] << 8 | tempbytes[1];
    return ((17572 * tempraw) >> 16) - 4685;
}


unsigned int SI7021::getHumidityPercent() {
    byte humbytes[2];
    _command(RH_READ, humbytes);
    long humraw = (long)humbytes[0] << 8 | humbytes[1];
    return ((125 * humraw) >> 16) - 6;
}

unsigned int SI7021::getHumidityBasisPoints() {
    byte humbytes[2];
    _command(RH_READ, humbytes);
    long humraw = (long)humbytes[0] << 8 | humbytes[1];
    return ((12500 * humraw) >> 16) - 600;
}

void SI7021::_command(byte * cmd, byte * buf ) {
    _writeReg(cmd, sizeof cmd);
    _readReg(buf, sizeof buf);
}

void SI7021::_writeReg(byte * reg, int reglen) {
    Wire.beginTransmission(I2C_ADDR);
    for(int i = 0; i < reglen; i++) {
        reg += i;
        Wire.write(*reg); 
    }
    Wire.endTransmission();
}

int SI7021::_readReg(byte * reg, int reglen) {
    Wire.requestFrom(I2C_ADDR, reglen);
    while(Wire.available() < reglen) {
    }
    for(int i = 0; i < reglen; i++) { 
        reg[i] = Wire.read(); 
    }
    return 1;
}

//note this has crc bytes embedded, per datasheet, so provide 12 byte buf
// DEPRECATED, keeping to avoid breaking linked code
int SI7021::getSerialBytes(byte * buf) {
    _writeReg(SERIAL1_READ, sizeof SERIAL1_READ);
    _readReg(buf, 6);
 
    _writeReg(SERIAL2_READ, sizeof SERIAL2_READ);
    _readReg(buf + 6, 6);
    
    // could verify crc here and return only the 8 bytes that matter
    return 1;
}

// we ignore CRC here, provide 8 byte buffer
// use this instead of getSerialBytes
int SI7021::getEightByteSerial(byte * buf) {
    // note we are ignoring CRC bytes
    byte serial[8];
    _writeReg(SERIAL1_READ, sizeof SERIAL1_READ);
    _readReg(serial, 8);
    buf[0] = serial[0]; //SNA_3
    buf[1] = serial[2]; //SNA_2
    buf[2] = serial[4]; //SNA_1
    buf[3] = serial[6]; //SNA_0

    _writeReg(SERIAL2_READ, sizeof SERIAL2_READ);
    _readReg(serial, 6);
    buf[4] = serial[0]; //SNB_3, AKA SI device type ID
    buf[5] = serial[1]; //SNB_2
    buf[6] = serial[3]; //SNB_1
    buf[7] = serial[4]; //SNB_0

    return 1;
}

int SI7021::getDeviceId() {
    byte serial[8];
    getEightByteSerial(serial);
    int id = serial[4];
    return id;
}

void SI7021::setHeater(bool on) {
    byte userbyte;
    if (on) {
        userbyte = 0x3E;
    } else {
        userbyte = 0x3A;
    }
    byte userwrite[] = {USER1_WRITE[0], userbyte};
    _writeReg(userwrite, sizeof userwrite);
}

// get humidity, then get temperature reading from humidity measurement
struct si7021_env SI7021::getHumidityAndTemperature() {
    si7021_env ret = {0, 0, 0};
    ret.humidityBasisPoints      = getHumidityBasisPoints();
    ret.celsiusHundredths        = _getCelsiusPostHumidity();
    ret.fahrenheitHundredths     = (1.8 * ret.celsiusHundredths) + 3200;
    return ret;
}
