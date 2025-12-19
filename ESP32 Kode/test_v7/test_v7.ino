#include "Sampler.h"


static void floatToExcelCSV(float v, char *out, size_t outSize)
{
    dtostrf(v, 0, 6, out);
    for (size_t i = 0; out[i] != '\0'; i++)
        if (out[i] == '.') out[i] = ',';
}


Sampler sampler;

const int TRIG_PIN = 18;
volatile bool trigFlag = false;

void IRAM_ATTR trigISR() {
    sampler.onTriggerISR();
    trigFlag = true;
}

void setup() {
    Serial.begin(115200);
    delay(300);

    if (!sampler.begin()) {
        Serial.println("Sampler begin failed");
        while (true) {}
    }

    pinMode(TRIG_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(TRIG_PIN), trigISR, RISING);

    Serial.println("Ready; waiting for trigger...");
}

void loop() {
    sampler.service();

    if (trigFlag) {
        trigFlag = false;

        const size_t N = 1024;
        static int32_t frames[N * 2];

        size_t got = 0;
        while (got == 0) {
            sampler.service();
            got = sampler.fetchFromTrigger(frames, N);
        }

        Serial.println("index;codeL;codeR;vL;vR");

        char bufL[32], bufR[32];

        for (size_t i = 0; i < got; i++) {
            int32_t codeL = frames[i*2 + 0];
            int32_t codeR = frames[i*2 + 1];

            float vL = sampler.codeToVoltage(codeL);
            float vR = sampler.codeToVoltage(codeR);

            floatToExcelCSV(vL, bufL, sizeof(bufL));
            floatToExcelCSV(vR, bufR, sizeof(bufR));

            Serial.print(i);
            Serial.print(';');
            Serial.print(bufL);
            Serial.print(';');
            Serial.println(bufR);
        }

        Serial.println("---- DONE ----");
    }
}
