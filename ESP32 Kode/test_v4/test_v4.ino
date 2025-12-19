#include <Arduino.h>
#include "PCNTSampler.h"

// Match your wiring
static const int PIN_BCLK    = 26;   // I2S BCK
static const int PIN_LRCLK   = 25;   // I2S WS / LRCLK
static const int PIN_DATAIN  = 33;   // I2S DIN from PCM1809
static const int PIN_TRIGGER = 18;   // trigger input (rising edge)

static const int CAPTURE_FRAMES = 40;

PCNTSampler sampler(PIN_BCLK,
                    PIN_LRCLK,
                    PIN_DATAIN,
                    PIN_TRIGGER,
                    CAPTURE_FRAMES);

void IRAM_ATTR triggerISR()
{
    sampler.onTriggerISR();
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("==== PCNTSampler TEST (flush+align+capture) ====");

    if (!sampler.begin()) {
        Serial.println("ERROR: sampler.begin() failed!");
        while (1) {
            delay(1000);
        }
    }

    pinMode(PIN_TRIGGER, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_TRIGGER), triggerISR, RISING);

    Serial.println("Sampler initialized. Waiting for trigger on GPIO18...");
}

void loop()
{
    // Run the sampler state machine
    bool ready = sampler.service();

    if (ready) {
        const float* buf = sampler.getBuffer();
        int frames = sampler.getCapturedFrames();

        Serial.println("=== CAPTURE COMPLETE ===");
        Serial.print("Frames captured: ");
        Serial.println(frames);

        int toPrint = (frames < 40) ? frames : 40;
        for (int i = 0; i < toPrint; ++i) {
            float vL = buf[i * 2 + 0];
            float vR = buf[i * 2 + 1];

            Serial.print(i);
            Serial.print(":  L=");
            Serial.print(vL, 6);
            Serial.print("  R=");
            Serial.println(vR, 6);
        }
        Serial.println("---- END OF CAPTURE ----");
        Serial.println("Waiting for next trigger...");
    }

    // Tight loop is fine; service() is mostly non-blocking.
    // You can add a tiny delay if needed:
    // delay(1);
}
