#include "Ultrasonic_sender.h"



UltrasonicSender us(
  13, 12, 14, 27,   // R-2R
  33,               // 40 kHz PWM
  25                // PCNT input (anden pin!)
);


void setup() {
    Serial.begin(115200);

    us.begin();
    delay(400);

    // Sæt amplitude (0–15)
    us.setAmplitude(6);
}

void loop() {
    // Kaldes hele tiden så klassen kan tælle pulser
    static unsigned long last_millis = 0;
    unsigned long now = millis();
    if (now - last_millis >= 1000)
    {
        us.sendPulses(25);
        last_millis = now;
    }
}
