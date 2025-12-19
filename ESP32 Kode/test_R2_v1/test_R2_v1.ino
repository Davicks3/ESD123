#include <Arduino.h>
#include "Sampler.h"

// I2S pins
static const int PIN_I2S_BCK  = 26;
static const int PIN_I2S_WS   = 25;
static const int PIN_I2S_DIN  = 33;
static const int PIN_MARKER   = 27;   // goes to your transistor circuit
static const int PIN_TRIGGER  = 18;   // external trigger in

Sampler sampler(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DIN, PIN_MARKER);

// How many frames we want to capture (~5.3 ms at 192 kHz)
static const size_t FRAMES_TO_CAPTURE = 1024;

// Move this buffer OUT of loop() to avoid stack overflow
static float g_captureBuffer[FRAMES_TO_CAPTURE * 2];  // L/R per frame

volatile bool g_triggeredFlag = false;

// ISR for external trigger
void IRAM_ATTR triggerISR()
{
    sampler.onTriggerISR();
    g_triggeredFlag = true;
}

void floatToExcelStr(float v, char *out, int outSize)
{
    dtostrf(v, 0, 6, out); // 6 decimals
    for (int i = 0; out[i] != '\0'; ++i) {
        if (out[i] == '.') out[i] = ',';
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println("Initializing sampler...");
    if (!sampler.begin()) {
        Serial.println("Sampler.begin FAILED");
        while (true) { delay(1000); }
    }
    Serial.println("Sampler initialized.");

    pinMode(PIN_TRIGGER, INPUT); // adjust PULLUP/PULLDOWN if needed
    attachInterrupt(digitalPinToInterrupt(PIN_TRIGGER), triggerISR, RISING);

    Serial.println("Waiting for trigger on GPIO18...");
}

void loop()
{
    // Keep the software ring updated
    sampler.update();

    static bool alreadyCaptured = false;

    if (g_triggeredFlag && !alreadyCaptured) {

        // Wait until enough post-trigger frames exist in the ring
        if (sampler.framesSinceTrigger() >= FRAMES_TO_CAPTURE) {

            size_t got = sampler.copyFromTrigger(g_captureBuffer, FRAMES_TO_CAPTURE);
            if (got == FRAMES_TO_CAPTURE) {
                alreadyCaptured = true;
                Serial.println("index;V_L;V_R");

                char bufL[32], bufR[32];
                for (size_t i = 0; i < FRAMES_TO_CAPTURE; ++i) {
                    float vL = g_captureBuffer[2 * i + 0];
                    float vR = g_captureBuffer[2 * i + 1];

                    floatToExcelStr(vL, bufL, sizeof(bufL));
                    floatToExcelStr(vR, bufR, sizeof(bufR));

                    Serial.print(i);
                    Serial.print(';');
                    Serial.print(bufL);
                    Serial.print(';');
                    Serial.println(bufR);
                }

                Serial.println("---- DONE ----");
            }
        }
    }

    // If you want multiple captures per boot, add a reset condition here:
    // if (alreadyCaptured && some_condition) {
    //     alreadyCaptured = false;
    //     g_triggeredFlag = false;
    // }
}
