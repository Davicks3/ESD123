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

void sendMessage(MessageId id) {
    // This is the time-critical “send now” path.
    // Under the hood: SIDLE, flush, write 2 bytes, STX, and wait TXFIFO empty.
    radio.sendByte(static_cast<uint8_t>(id));
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("CC1101 TX (packet fast) starting...");

    radio.begin();
    Serial.println("Radio initialized (TX).");
}

void loop() {
    // Example test: cycle through messages every 100 ms
    static uint32_t last = 0;
    static uint8_t idx   = 0;

    uint32_t now = millis();
    if (now - last > 100) {
        last = now;

        MessageId id = static_cast<MessageId>(idx % 5);
        idx++;

        Serial.print("Sending code: ");
        Serial.println((int)id);

        sendMessage(id);
    }

    // In your real code, you just call sendMessage(MSG_XXX) when you decide to send.
}