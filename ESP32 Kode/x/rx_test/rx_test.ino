#include <Arduino.h>
#include "CC1101_Packet.h"

constexpr uint8_t PIN_CC1101_CS = 5;

CC1101_Packet radio(PIN_CC1101_CS);

enum MessageId : uint8_t {
    MSG_STOP  = 0,
    MSG_GO    = 1,
    MSG_LEFT  = 2,
    MSG_RIGHT = 3,
    MSG_PING  = 4
};

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("CC1101 RX (packet fast) starting...");

    radio.begin();
    radio.enterRx();
    Serial.println("Radio in RX, waiting...");
}

void loop() {
    uint8_t b;
    int n = radio.receiveByte(&b);

    if (n == 1) {
        switch (b) {
        case MSG_STOP:  Serial.println("Received: STOP");  break;
        case MSG_GO:    Serial.println("Received: GO");    break;
        case MSG_LEFT:  Serial.println("Received: LEFT");  break;
        case MSG_RIGHT: Serial.println("Received: RIGHT"); break;
        case MSG_PING:  Serial.println("Received: PING");  break;
        default:
            Serial.print("Received unknown byte: ");
            Serial.println((int)b);
            break;
        }
    }

    // tight loop; no delay() so we don't add software latency
}