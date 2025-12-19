#include "Ultrasonic_sender.h"

// R2R: b0, b1, b2, b3   +   40 kHz output pin
UltrasonicSender us(13, 12, 14, 27, 33);

void setup() {
    Serial.begin(115200);
    us.begin();
    delay(400);

    // Sæt amplitude (0–15)
    us.setAmplitude(15);

    // Send 200 pulser
    us.sendPulses(65000);
}

void loop() {
    // Kaldes hele tiden så klassen kan tælle pulser
    us.update();
}
