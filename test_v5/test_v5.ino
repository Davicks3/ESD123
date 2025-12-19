#include <Arduino.h>
#include "Sampler.h"

// --- Pin definitions (adjust if needed) ---
static const int PIN_I2S_BCK   = 26;
static const int PIN_I2S_WS    = 25;  // LRCLK / FSYNC
static const int PIN_I2S_DIN   = 33;  // PCM1809 DOUT
static const int PIN_TRIGGER   = 18;  // external trigger

// --- Global sampler instance ---
Sampler sampler(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DIN);

// --- Trigger flag ---
volatile bool gTriggered = false;

void IRAM_ATTR onTrigger()
{
    gTriggered = true;
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println("Initializing sampler...");
    if (!sampler.begin()) {
        Serial.println("Sampler.begin() failed, halting.");
        while (true) { delay(1000); }
    }

    pinMode(PIN_TRIGGER, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_TRIGGER), onTrigger, RISING);

    Serial.println("Sampler initialized. Waiting for trigger on GPIO18...");
}

void loop()
{
    static bool captured = false;
    const size_t FRAMES_TO_CAPTURE = 40;
    static float samples[FRAMES_TO_CAPTURE * 2];  // L/R interleaved

    if (!gTriggered) {
        // No trigger yet: continuously discard small chunks
        // to keep latency small when trigger eventually happens.
        sampler.discardChunk();
        return;
    }
    gTriggered = false;

    size_t got = sampler.capture(samples, FRAMES_TO_CAPTURE);
    Serial.print("Frames captured: ");
    Serial.println(got);

    for (size_t i = 0; i < got; ++i) {
        float vL = samples[i * 2 + 0];
        float vR = samples[i * 2 + 1];

        Serial.print("Frame ");
        Serial.print(i);
        Serial.print(": L=");
        Serial.print(vL, 6);
        Serial.print(" V  R=");
        Serial.print(vR, 6);
        Serial.println(" V");
    }

    Serial.println("---- DONE ----");

    // If you want repeated captures:
    // - comment out the 'captured' logic above
    // - reset flags here, e.g.:
    // gTriggered = false;
    // captured   = false;
}
