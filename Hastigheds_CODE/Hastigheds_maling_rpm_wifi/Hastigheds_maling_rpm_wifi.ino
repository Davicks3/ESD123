#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// ====== MODTAGERENS MAC (SKIFT DENNE) ======
uint8_t receiverMAC[] = {0x84, 0x1F, 0xE8, 0x32, 0x02, 0xB8};

// ====== Indstillinger ======
const float pulsesPerRev = 30.0f;
const float cmPerPulse   = 1.173f;
const unsigned long sendEveryMs = 50;

// ====== Telemetri pakke ======
typedef struct __attribute__((packed)) {
  uint32_t timeMs;
  float rpmL;
  float rpmR;
  float cmL;
  float cmR;
} TelemetryPacket;

TelemetryPacket tx;

// ====== ISR variabler ======
volatile unsigned long lastPulseL_us = 0;
volatile unsigned long prevPulseL_us = 0;
volatile unsigned long pulseCountL   = 0;

volatile unsigned long lastPulseR_us = 0;
volatile unsigned long prevPulseR_us = 0;
volatile unsigned long pulseCountR   = 0;

// ====== ISR ======
void IRAM_ATTR sensorL() {
  unsigned long now = micros();
  prevPulseL_us = lastPulseL_us;
  lastPulseL_us = now;
  pulseCountL++;
}

void IRAM_ATTR sensorR() {
  unsigned long now = micros();
  prevPulseR_us = lastPulseR_us;
  lastPulseR_us = now;
  pulseCountR++;
}

// ====== Virtuel RPM ======
float computeVirtualRPM(unsigned long prev,
                        unsigned long last,
                        unsigned long now_us) {
  if (prev == 0 || last == 0) return 0.0f;

  float interval_us = (float)(last - prev);
  if (interval_us <= 0) return 0.0f;

  if (now_us <= last) {
    return (60.0f * 1e6) / (interval_us * pulsesPerRev);
  }

  float elapsed_us = (float)(now_us - last);
  return (60.0f * 1e6) / ((elapsed_us + interval_us) * pulsesPerRev);
}

void setup() {
  Serial.begin(115200);

  // WiFi skal være STA for ESP-NOW
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init fejlede");
    while (true);
  }

  // Tilføj peer
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverMAC, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Kunne ikke tilføje peer");
    while (true);
  }

  pinMode(2, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(2), sensorL, CHANGE);
  attachInterrupt(digitalPinToInterrupt(4), sensorR, CHANGE);

  Serial.println("ESP-NOW SENDER klar");
}

void loop() {
  static unsigned long lastSendMs = 0;

  unsigned long pL_prev, pL_last, cntL;
  unsigned long pR_prev, pR_last, cntR;

  noInterrupts();
  pL_prev = prevPulseL_us;
  pL_last = lastPulseL_us;
  cntL    = pulseCountL;

  pR_prev = prevPulseR_us;
  pR_last = lastPulseR_us;
  cntR    = pulseCountR;
  interrupts();

  unsigned long now_us = micros();

  tx.timeMs = millis();
  tx.rpmL   = computeVirtualRPM(pL_prev, pL_last, now_us);
  tx.rpmR   = computeVirtualRPM(pR_prev, pR_last, now_us);
  tx.cmL    = cntL * cmPerPulse;
  tx.cmR    = cntR * cmPerPulse;

  if (millis() - lastSendMs >= sendEveryMs) {
    lastSendMs = millis();
    esp_now_send(receiverMAC,
                 (uint8_t*)&tx,
                 sizeof(tx));
  }
}
