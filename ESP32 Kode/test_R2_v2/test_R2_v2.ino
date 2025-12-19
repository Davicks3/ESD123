#include <Arduino.h>
#include "Sampler.h"

// ---- Pin configuration ----
static const int PIN_I2S_BCK   = 26;
static const int PIN_I2S_WS    = 25;  // LRCLK
static const int PIN_I2S_DIN   = 33;
static const int PIN_TRIGGER   = 18;

// ---- Capture configuration ----
static const size_t CAPTURE_FRAMES = 1024;   // number of frames to export each time
static float g_capture[CAPTURE_FRAMES * 2];  // [L,R,L,R,...] as volts

Sampler sampler(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DIN);

// Just to know a trigger happened (for debug, not strictly necessary)
volatile bool g_triggeredISR = false;

void IRAM_ATTR triggerISR()
{
    sampler.onTriggerISR();
    g_triggeredISR = true;
}

// Convert float to Excel-"friendly" string: "1,234567"
void floatToExcelStr(float v, char *out, int outSize)
{
    dtostrf(v, 0, 6, out); // 6 decimals
    for (int i = 0; out[i] != '\0'; ++i) {
        if (out[i] == '.') {
            out[i] = ',';
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    pinMode(PIN_TRIGGER, INPUT_PULLDOWN);

    if (!sampler.begin()) {
        Serial.println("Sampler.begin FAILED");
        while (1) {
            delay(1000);
        }
    }

    // Marker threshold: adjust if you like (e.g. -0.1, -0.3, etc.)
    sampler.setMarkerThreshold(-0.2f);

    attachInterrupt(digitalPinToInterrupt(PIN_TRIGGER), triggerISR, RISING);

    Serial.println("Sampler initialized.");
    Serial.println("Waiting for trigger on GPIO18...");
}

void loop()
{
    // Keep software ring buffer filled
    sampler.update();

    // If we don't have a trigger yet, nothing else to do
    if (!sampler.hasTrigger()) {
        return;
    }

    // Wait until we have at least this many frames AFTER trigger
    // so there's enough post-marker data available.
    const uint32_t MIN_FRAMES_AFTER_TRIGGER = CAPTURE_FRAMES + 64;

    uint32_t sinceTrig = sampler.framesSinceTrigger();
    if (sinceTrig < MIN_FRAMES_AFTER_TRIGGER) {
        return;
    }

    // Try to copy from aligned marker into g_capture.
    size_t got = sampler.copyFromAlignedMarker(g_capture, CAPTURE_FRAMES);
    if (got != CAPTURE_FRAMES) {
        // Not enough data yet (race), try again next loop
        return;
    }

    // ---- Print CSV ----
    Serial.println("index;V_L;V_R");
    char bufL[32], bufR[32];

    for (size_t i = 0; i < CAPTURE_FRAMES; ++i) {
        float vL = g_capture[2 * i + 0];
        float vR = g_capture[2 * i + 1];

        floatToExcelStr(vL, bufL, sizeof(bufL));
        floatToExcelStr(vR, bufR, sizeof(bufR));

        Serial.print(i);
        Serial.print(';');
        Serial.print(bufL);
        Serial.print(';');
        Serial.println(bufR);
    }
    Serial.println("---- DONE ----");

    // Re-arm for the next capture
    sampler.clearTrigger();
    g_triggeredISR = false;
    Serial.println("Re-armed; waiting for next trigger...");
}