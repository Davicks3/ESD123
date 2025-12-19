#include <Arduino.h>
#include "Sampler.h"

static const int TRIGGER_PIN = 18;

// 1024 frames * 2 channels
float samples[Sampler::CAPTURE_FRAMES * 2];

Sampler sampler;
volatile bool triggerRequested = false;

void IRAM_ATTR onTriggerISR() {
    // Just set a flag – the heavy work happens in loop()
    triggerRequested = true;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=== Sampler 1024-frame test ===");

    if (!sampler.begin()) {
        Serial.println("Sampler.begin() FAILED");
        while (true) {
            delay(1000);
        }
    }

    pinMode(TRIGGER_PIN, INPUT);  // or INPUT_PULLDOWN if you need that
    attachInterrupt(digitalPinToInterrupt(TRIGGER_PIN), onTriggerISR, RISING);

    Serial.print("Sampler initialized. Waiting for triggers on GPIO");
    Serial.println(TRIGGER_PIN);
}

void loop() {
    if (triggerRequested) {
        triggerRequested = false;

        size_t got = sampler.capture1024(samples);

        Serial.print("Frames captured: ");
        Serial.println(got);

        // Print first ~20 frames so you can see structure
        size_t toPrint = (got < 20) ? got : 20;
        for (size_t i = 0; i < toPrint; ++i) {
            float vL = samples[2*i + 0];
            float vR = samples[2*i + 1];

            Serial.print("Frame ");
            Serial.print(i);
            Serial.print(": L=");
            Serial.print(vL, 6);
            Serial.print(" V  R=");
            Serial.print(vR, 6);
            Serial.println(" V");
        }

        Serial.println("---- CAPTURE DONE ----");
        Serial.println("Waiting for next trigger...");
    }

    // Idle
}