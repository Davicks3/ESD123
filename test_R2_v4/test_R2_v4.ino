#include <Arduino.h>
#include "Sampler.h"

// ---------------- PIN DEFINES (ADJUST TO YOUR BOARD) ----------------

// I2S pins for PCM1809
static const gpio_num_t PIN_BCLK   = GPIO_NUM_26;   // BCK
static const gpio_num_t PIN_LRCLK  = GPIO_NUM_25;   // LRCLK / WS
static const gpio_num_t PIN_DATAIN = GPIO_NUM_33;   // SD (ADC -> ESP32)

// Transistor pulse pin (alignment sync)
static const gpio_num_t PIN_SYNC_PULSE = GPIO_NUM_17;

// External trigger pin
static const int TRIGGER_PIN = 18;  // GPIO18

// ---------------- GLOBALS ----------------

Sampler sampler(PIN_BCLK, PIN_LRCLK, PIN_DATAIN, PIN_SYNC_PULSE);

// Capture buffers (global to avoid stack overflow)
static float g_left[FRAMES_PER_SIGNAL];
static float g_right[FRAMES_PER_SIGNAL];

// ---------------- ISR ----------------

void IRAM_ATTR onTriggerISR() {
    sampler.trigger();
}

// ---------------- SETUP ----------------

void setup() {
    Serial.begin(115200);
    delay(500);

    // Attach external trigger
    attachInterrupt(digitalPinToInterrupt(TRIGGER_PIN), onTriggerISR, RISING);

    Serial.println();
    Serial.println("=== Sampler Test ===");

    pinMode(TRIGGER_PIN, INPUT_PULLDOWN);

    Serial.println("Initializing sampler...");
    if (!sampler.begin()) {
        Serial.println("Sampler.begin FAILED");
        while (true) {
            delay(1000);
        }
    }

    // Discard first ~100 ms to let ADC + analog settle
    sampler.discard_initial();
    Serial.println("Sampler::begin done (ADC settled)");
    Serial.println("Sampler initialized.");

    Serial.println("Waiting for trigger on GPIO18...");
}

// ---------------- LOOP ----------------

void loop() {

    // If capturing, try to fetch one full signal
    if (sampler.get_triggered_state()) {
        Serial.println("Triggered");
        uint16_t offsetFrames = 0;
        size_t   frames = sampler.fetch(g_left, g_right, &offsetFrames);

        if (frames > 0) {
            // We got a capture – print CSV and allow another capture
            Serial.print("Captured frames: ");
            Serial.println(frames);
            Serial.print("Trigger offset (frames late if >0): ");
            Serial.println(offsetFrames);
            bool found_sig_l = false;
            bool found_sig_r = false;
            for (int t = 0; t < frames; t++)
            {
                if (!found_sig_l && g_left[t] > SYNC_PULSE_THRESHOLD)
                {
                    Serial.print("Trigger delay - left: "); Serial.print(t); Serial.println(" frames");
                    found_sig_l = true;
                }
                
                if (!found_sig_r && g_right[t] > SYNC_PULSE_THRESHOLD)
                {
                    Serial.print("Trigger delay - right: "); Serial.print(t); Serial.println(" frames");
                    found_sig_r = true;
                }
                if (found_sig_l && found_sig_r) { break; }
            }
            // Print header for CSV
            Serial.println("index,time_s,left_V,right_V");

            for (size_t i = 0; i < frames; ++i) {
                float t = (float)i / (float)SAMPLE_RATE;
                Serial.print(i);
                Serial.print(",");
                Serial.print(t, 9);          // high-res time
                Serial.print(",");
                Serial.print(g_left[i], 6);
                Serial.print(",");
                Serial.println(g_right[i], 6);
            }

            Serial.println("---- DONE ----");
            Serial.println("Waiting for next trigger...");
        }

        // If frames == 0, just let loop() run again; sampler.fetch()
        // will keep reading until it can fill the requested frames.
    }

    // Keep frame counter & sync logic running
    sampler.handle();
}
