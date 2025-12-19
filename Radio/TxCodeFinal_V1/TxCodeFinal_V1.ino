#include "CC1101.h"
#include "Ultrasonic_sender.h"

// Define Pins
#define PIN_SCK   18
#define PIN_MISO  19
#define PIN_MOSI  23
#define PIN_CSN   5
#define PIN_GDO0  17

// Instantiate Class
CC1101 radio(PIN_CSN, PIN_GDO0, PIN_MISO, PIN_MOSI, PIN_SCK);
UltrasonicSender us(13, 12, 14, 27, 33, 25);

// --- Tasks ---
void radioTxTask(void *pvParameters);
void speedTask(void *pvParameters);

// --- Handles ---
TaskHandle_t radioTaskHandle;
TaskHandle_t speedTaskHandle;

// --- Variables ---
volatile int speed = 0;
unsigned long lastTime = 0;

// --- Test setup ---
#define speedReadPin 4  // idk skal ændres
#define ledPin 2

#define SAMPLE_FREQ 1
#define SAMPLE_MS 1.0f / SAMPLE_FREQ * 1000.0f

// --- Initialize Code ---
void setup() { 
  Serial.begin(115200);
  delay(3000);
  us.begin(); delay(500);

  us.setAmplitude(8); //Ultralyd sendespænding

  Serial.println("\n--- CC1101 TX (500k) ---");

  if (!radio.begin()) {
    Serial.println("CRITICAL FAILURE: Chip not responding.");
    while (1);
  }

  Serial.println("Code Starting");

  //us.setAmplitude(15);

  pinMode(speedReadPin, INPUT);
  pinMode(ledPin, OUTPUT);

  // --- Tasks Definitions ---
  xTaskCreatePinnedToCore(
    radioTxTask,                  // Task function
    "Radio Transmit Task",        // Task name
    4096,                         // Stack size
    NULL,                         // Parameters
    5,                            // Priority
    &radioTaskHandle,             // Task handle
    0                             // Core ID
  );

  xTaskCreatePinnedToCore(
    speedTask,                    // Task function
    "Speed Controller Task",      // Task name
    4096,                         // Stack size
    NULL,                         // Parameters
    1,                            // Priority
    &speedTaskHandle,             // Task handle
    1                             // Core ID
  );
}

// --- Do Nothing Code ---
void loop() {
  vTaskDelay(portMAX_DELAY);
}

void radioTxTask(void *pvParameters) {
  for (;;) {
    char msg[11];
    sprintf(msg, "%010d", speed);

    Serial.printf("TX: %s\n", msg);
    radio.sendPacket((uint8_t *)msg, 10);
    us.sendPulses(25);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void speedTask(void *pvParameters) {
  for (;;) {
    // Read the pin
    //int rawValue = analogRead(speedReadPin);
    speed = millis();
    // Update the global variable
  //  speed = rawValue;

    // Read rate (e.g., update sensor every 10ms)
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
