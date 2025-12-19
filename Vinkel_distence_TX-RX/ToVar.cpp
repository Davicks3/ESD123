#include "ToVar.h"

ToVar::ToVar(HardwareSerial* ser) {
    serial = ser;
}

void ToVar::sendVars(int16_t angle, uint32_t distance) {
    // grænseværdier
    if (angle > 90) angle = 90;
    if (angle < -90) angle = -90;
    if (distance > 100000) distance = 100000;
    if (distance < 0) distance = 0;

    uint16_t checksum = (uint16_t)(angle + distance);

    uint8_t frame[FRAME_SIZE];
    frame[0] = START_BYTE;

    memcpy(&frame[1], &angle, sizeof(angle));
    memcpy(&frame[3], &distance, sizeof(distance));
    memcpy(&frame[7], &checksum, sizeof(checksum));

    frame[9] = END_BYTE;

    serial->write(frame, FRAME_SIZE);
}

bool ToVar::receiveVars(int16_t &angle, uint32_t &distance) {
    static uint8_t buffer[FRAME_SIZE];
    static size_t index = 0;

    while (serial->available()) {
        uint8_t byteIn = serial->read();

        // Venter på start_bit
        if (index == 0) {
            if (byteIn == START_BYTE) {
                buffer[index++] = byteIn;
            }
            continue;
        }

        buffer[index++] = byteIn;

        if (index == FRAME_SIZE) {
            index = 0;

            // Venter på slut_bit
            if (buffer[9] != END_BYTE) {
                return false;
            }

            memcpy(&angle, &buffer[1], sizeof(angle));
            memcpy(&distance, &buffer[3], sizeof(distance));

            uint16_t receivedChecksum;
            memcpy(&receivedChecksum, &buffer[7], sizeof(receivedChecksum));

            uint16_t calculatedChecksum = (uint16_t)(angle + distance);

            if (receivedChecksum != calculatedChecksum) {
                return false;
            }

            return true;
        }
    }

    return false;
}