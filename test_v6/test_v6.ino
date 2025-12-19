// test.ino
#include <Arduino.h>
#include "I2SRingSampler.h"

// ---- Pins (adjust if needed) ----
static const int PIN_I2S_BCK   = 26;
static const int PIN_I2S_WS    = 25;
static const int PIN_I2S_DIN   = 33;
static const int PIN_TRIGGER   = 18;

// ---- Ring / capture settings ----
static const uint32_t RING_FRAMES    = 4096;  // ~21.3 ms history
static const uint32_t CAPTURE_FRAMES = 1024;  // ~5.33 ms window

I2SRingSampler sampler(RING_FRAMES);

// Trigger state
volatile bool     gTriggered      = false;
volatile uint32_t gTriggerIndex   = 0;
volatile bool     gCaptureDone    = false;

// For one-shot behaviour
bool captureInProgress = false;

// Excel-friendly float → string with comma
void floatToExcelStr(float v, char* out, int outSize)
{
    dtostrf(v, 0, 6, out);
    for (int i = 0; out[i] != '\0'; ++i) {
        if (out[i] == '.') out[i] = ',';
    }
}

void IRAM_ATTR onTrigger()
{
    if (!captureInProgress && !gCaptureDone) {
        // Mark trigger index in sampler
        sampler.markTrigger();
        gTriggerIndex = sampler.getTriggerIndex();
        gTriggered = true;
        captureInProgress = true;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    pinMode(PIN_TRIGGER, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(PIN_TRIGGER), onTrigger, RISING);

    Serial.println("Initializing I2S sampler...");
    if (!sampler.begin(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DIN)) {
        Serial.println("sampler.begin() failed!");
        while (true) { delay(1000); }
    }
    Serial.println("Sampler initialized. Waiting for trigger on GPIO18...");
}

void loop()
{
    // Always keep the ring buffer filled
    sampler.service();

    if (gTriggered && !gCaptureDone) {
        // Check if we have enough frames after trigger
        if (sampler.hasPostTriggerWindow(CAPTURE_FRAMES)) {
            // Copy window to local buffer as voltages
            static float capture[CAPTURE_FRAMES * 2];

            if (sampler.extractWindow(capture, CAPTURE_FRAMES)) {
                gCaptureDone  = true;
                gTriggered    = false;
                captureInProgress = false;

                Serial.println("index;V_L;V_R");
                char bufVL[32], bufVR[32];

                for (uint32_t i = 0; i < CAPTURE_FRAMES; ++i) {
                    float vL = capture[2 * i + 0];
                    float vR = capture[2 * i + 1];

                    floatToExcelStr(vL, bufVL, sizeof(bufVL));
                    floatToExcelStr(vR, bufVR, sizeof(bufVR));

                    Serial.print(i);
                    Serial.print(';');
                    Serial.print(bufVL);
                    Serial.print(';');
                    Serial.println(bufVR);
                }

                Serial.println("---- DONE ----");
                // If you want repeated captures, reset this:
                gCaptureDone = false;
            }
        }
    }

    // Nothing else; loop() is largely consumed by sampler.service()
}