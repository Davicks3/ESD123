#include <Arduino.h>
#include "WritePointer.h"
#include "Sampler.h"

static const int PIN_BCLK    = 26;
static const int PIN_LRCLK   = 25;
static const int PIN_DATA    = 33;
static const int PIN_PCNT    = 14;  // wired to LRCLK
static const int PIN_TRIGGER = 18;

WritePointer writePtr(PIN_PCNT, PCNT_UNIT_0);
Sampler      sampler(PIN_BCLK, PIN_LRCLK, PIN_DATA, writePtr);

volatile bool triggerFlag = false;

void IRAM_ATTR trigger_isr()
{
    triggerFlag = true;
    sampler.trigger();
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    pinMode(PIN_TRIGGER, INPUT);

    Serial.println("Initializing writePtr...");
    if (!writePtr.begin()) {
        Serial.println("writePtr.begin() FAILED");
        while (1) delay(1000);
    }

    Serial.println("Initializing sampler...");
    if (!sampler.begin()) {
        Serial.println("sampler.begin() FAILED");
        while (1) delay(1000);
    }
    Serial.println("Sampler initialized.");

    attachInterrupt(digitalPinToInterrupt(PIN_TRIGGER), trigger_isr, RISING);
    Serial.println("Waiting for trigger on GPIO18...");
}

void loop()
{
    // ---- keep write pointer in sync with real LRCLK ----
    static uint32_t lastWPms = 0;
    if (micros() - lastWPms >= 1000) {      // every 1 ms
        lastWPms = micros();
        writePtr.update();
    }

    if (triggerFlag) {
        triggerFlag = false;

        //sampler.trigger();

        const size_t N_FRAMES = 40;
        float buf[N_FRAMES * 2];

        size_t got = sampler.fetch(buf, N_FRAMES);
        Serial.print("Frames fetched: ");
        Serial.println(got);

        for (size_t i = 0; i < got; ++i) {
            float vL = buf[i * 2 + 0];
            float vR = buf[i * 2 + 1];
            Serial.print("Frame ");
            Serial.print(i);
            Serial.print(": L=");
            Serial.print(vL, 6);
            Serial.print(" V  R=");
            Serial.print(vR, 6);
            Serial.println(" V");
        }

        Serial.println("---- DONE ----");
        while(true) {}
    }
}
