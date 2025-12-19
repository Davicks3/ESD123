// test.ino
#include <Arduino.h>
#include "Sampler.h"
#include "LRCLKCounter.h"

// Trigger on GPIO18
static const int TRIGGER_PIN = 18;

// LRCLK is on I2S_WS_PIN (25), and we tap it on 14 via a jumper
static const int LRCLK_TAP_PIN = 14;

// Globals
Sampler      sampler;
LRCLKCounter lrclk;

// Trigger flag
volatile bool g_triggered = false;

void IRAM_ATTR onTriggerISR()
{
    g_triggered = true;
    sampler.trigger();
}

// Update framesProduced from PCNT reading of LRCLK
void updateFramesProducedFromPCNT()
{
    static uint32_t lastMs = 0;
    const float interval_s = 0.05f;  // 50 ms

    uint32_t now = millis();
    if (now - lastMs < (uint32_t)(interval_s * 1000.0f)) {
        return;
    }
    lastMs = now;

    int16_t count = lrclk.readAndClear();
    if (count < 0) {
        count = 0;
    }

    // Each LRCLK edge = 1 stereo frame
    uint32_t frames = (uint32_t)count;
    sampler.onFramesProduced(frames);

    // No freq printing – we only care about frames for the tracker now.
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    pinMode(TRIGGER_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(TRIGGER_PIN), onTriggerISR, RISING);

    // Make sure LRCLK tap pin is input
    pinMode(LRCLK_TAP_PIN, INPUT);

    if (!lrclk.begin(LRCLK_TAP_PIN)) {
        Serial.println("LRCLKCounter.begin() FAILED");
        while (true) { delay(1000); }
    }

    if (!sampler.begin()) {
        Serial.println("Sampler.begin() FAILED");
        while (true) { delay(1000); }
    }

    Serial.println("Sampler initialized.");
    Serial.println("Make sure GPIO25 (LRCLK) is wired to GPIO14 (PCNT).");
    Serial.println("Waiting for trigger on GPIO18...");
}

void loop()
{
    // Track frames produced via LRCLK
    updateFramesProducedFromPCNT();

    if (g_triggered) {
        g_triggered = false;

        // Fetch 40 frames after trigger
        const size_t N_FRAMES = 40;
        float buf[2 * N_FRAMES] = {0.0f};

        size_t got = sampler.fetch(buf, N_FRAMES);
        Serial.print("Frames fetched: ");
        Serial.println(got);

        for (size_t i = 0; i < got; ++i) {
            float vL = buf[2 * i + 0];
            float vR = buf[2 * i + 1];
            Serial.print("Frame ");
            Serial.print(i);
            Serial.print(": L=");
            Serial.print(vL, 6);
            Serial.print(" V  R=");
            Serial.print(vR, 6);
            Serial.println(" V");
        }

        Serial.println("---- DONE ----");
    }
}
