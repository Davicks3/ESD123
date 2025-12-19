#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// ====== Samme struct som sender ======
typedef struct __attribute__((packed)) {
  uint32_t timeMs;
  float rpmL;
  float rpmR;
  float cmL;
  float cmR;
} TelemetryPacket;

TelemetryPacket rx;

// ====== Receive callback (NY API) ======
void onReceive(const esp_now_recv_info_t *info,
               const uint8_t *data,
               int len) {

  if (len != sizeof(TelemetryPacket)) return;

  memcpy(&rx, data, sizeof(rx));

  Serial.printf(
    "t=%lu ms | rpmL=%.2f rpmR=%.2f | cmL=%.2f cmR=%.2f\n",
    rx.timeMs, rx.rpmL, rx.rpmR, rx.cmL, rx.cmR
  );
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init fejlede");
    while (true);
  }

  esp_now_register_recv_cb(onReceive);

  Serial.print("MODTAGER MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  delay(10);
}
