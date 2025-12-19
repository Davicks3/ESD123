#ifndef ToVar_H
#define ToVar_H

#include <Arduino.h>

class DualVarComm {
private:
    HardwareSerial* serial;

    static const uint8_t START_BYTE = 0xAA;
    static const uint8_t END_BYTE   = 0x55;
    static const size_t FRAME_SIZE  = 10;

public:
    DualVarComm(HardwareSerial* ser);

    void sendVars(int16_t angle, uint32_t distance);

    // bool retunere hvis checksum passer
    bool receiveVars(int16_t &angle, uint32_t &distance);
};

#endif // ToVar_H