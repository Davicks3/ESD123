#include <Arduino.h>
#include "Algorithm.h"


// I2S pins for PCM1809
static const gpio_num_t PIN_BCLK   = GPIO_NUM_26;   // BCK
static const gpio_num_t PIN_LRCLK  = GPIO_NUM_25;   // LRCLK
static const gpio_num_t PIN_DATAIN = GPIO_NUM_33;   // DATA

// Transistor pulse pin
static const gpio_num_t PIN_SYNC_PULSE = GPIO_NUM_17;

// External trigger pin
static const int TRIGGER_PIN = 18;

Algorithm algorithm(PIN_BCLK, PIN_LRCLK, PIN_DATAIN, PIN_SYNC_PULSE);

void IRAM_ATTR onTriggerISR() {
    algorithm.trigger();
}


void setup() {
    Serial.begin(115200);
    //Serial.begin(500000);
    delay(500);

    pinMode(TRIGGER_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(TRIGGER_PIN), onTriggerISR, RISING);

    Serial.println();
    Serial.println("=== Sampler Test ===");

    Serial.println("Initializing sampler...");
    if (!algorithm.begin()) {
        Serial.println("Algorithm.begin FAILED");
        while (true) {
            delay(1000);
        }
    }
    
    algorithm.discard_initial();
    Serial.println("Algorithm::begin done (ADC settled)");
    delay(1000);

    float left[FRAMES_PER_READ], right[FRAMES_PER_READ];
    algorithm.discard_frames(1000);
    size_t _samples = algorithm.read_samples(left, right);

    for (size_t i = 0; i < _samples; i++)
    {
        Serial.println(right[i], 6);
    }
    while(true);
   
}

void loop()
{

    if (algorithm.get_triggered_state())
    {
        float angle, distance;
        algorithm.calculate(angle, distance);
        /*
        int count = 0;
        while(!algorithm.sync_indicies())
        {
            count++;
            if (count >= 10) { Serial.println("ERR0"); break; }
        }
        */
    }
    
    algorithm.handle();
}
