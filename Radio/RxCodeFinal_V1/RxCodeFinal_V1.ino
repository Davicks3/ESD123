#include "CC1101.h"
#include <ESP32Servo.h>

// --- Radio Pins ---
#define PIN_SCK 18
#define PIN_MISO 19
#define PIN_MOSI 23
#define PIN_CSN 5
#define PIN_GDO0 17

// --- Motor Controller Pins ---
#define PIN_RHB 33  // Right Half-bridge
#define PIN_REN 32  // Right Half-bridge Enable
#define PIN_LHB 25  // Left Half-bridge
#define PIN_LEN 26  // Left Half-bridge Enable

// --- Servo Controller Pins ---
#define PIN_SERVO 27

// --- Trigger Pin ---
#define trigger 4

// --- Tasks Handles ---
TaskHandle_t radioRxHandle    = NULL;
TaskHandle_t motorCtrlHandle  = NULL;
TaskHandle_t servoCtrlHandle  = NULL;

// --- Forward Declarations ---
void radioRx(void* pvParameters);
void motorCtrl(void* pvParameters);
void servoCtrl(void* pvParameters);

// --- Instantiate Class ---
CC1101 radio(PIN_CSN, PIN_GDO0, PIN_MISO, PIN_MOSI, PIN_SCK);
Servo myServo;

// --- Variables ---
volatile int speed = 0;


// ISR: Wakes up the task when packet arrives
void IRAM_ATTR isrGDO0() {
  vTaskNotifyGiveFromISR(radioRxHandle, NULL);
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  if (!radio.begin()) {
    Serial.println("CRITICAL FAILURE: Chip not responding.");
    while (1);
  }

  Serial.println("Code Starting");

  Initialize();   // Initialize Pins, H-bridge, Servo PWM, and Motor PWM
  radio.setRx();  // Initialize Radio as recevier

  Serial.println("RX Listening...");

  // --- Tasks Definitions ---
  xTaskCreatePinnedToCore(
    radioRx,               // Task function
    "Radio Receive Task",  // Task name
    4096,                  // Stack size
    NULL,                  // Parameters
    1,                     // Priority
    &radioRxHandle,        // Task handle
    0                      // Core ID
  );

  xTaskCreatePinnedToCore(
    motorCtrl,                // Task function
    "Motor Controller Task",  // Task name
    4096,                     // Stack size
    NULL,                     // Parameters
    2,                        // Priority
    &motorCtrlHandle,         // Task handle
    1                         // Core ID
  );

  xTaskCreatePinnedToCore(
    servoCtrl,                // Task function
    "Servo Controller Task",  // Task name
    4096,                     // Stack size
    NULL,                     // Parameters
    1,                        // Priority
    &servoCtrlHandle,         // Task handle
    1                         // Core ID
  );

  // Attach interrupt (Rising edge = Sync Word Received)
  attachInterrupt(digitalPinToInterrupt(PIN_GDO0), isrGDO0, RISING);
}

// --- Do Nothing Code ---
void loop() {
  vTaskDelay(portMAX_DELAY);
}

void radioRx(void* pvParameters) {
  for (;;) {
    digitalWrite(trigger, LOW);

    // 1. Wait for notification from ISR
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // 2. Buffer must be 11 bytes to hold 10 chars + 1 null terminator
    uint8_t buffer[10]; 
    uint8_t length;
    int8_t rssi;

    if (radio.checkPacket(buffer, length, rssi)) {
      // 3. Safety: Ensure the string ends with 0 so atoi works
      buffer[10] = '\0'; 
      
      // 4. Convert text to integer
      speed = atoi((char*)buffer);

      Serial.printf("RX: %d [RSSI: %d]\n", speed, rssi);

      // 6. Visual feedback (Moved INSIDE the loop)
      digitalWrite(trigger, HIGH);
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

void motorCtrl(void* pvParameters) {
  for (;;) {
    // Write the value received from the radio
    ledcWrite(PIN_REN, speed); 
    vTaskDelay(pdMS_TO_TICKS(50)); // Check for updates every 50ms
  }
}

void servoCtrl(void* pvParameters) {
  for (;;) {
    myServo.write(0);
    vTaskDelay(500);
    myServo.write(180);
    vTaskDelay(500);
  }
}

void Initialize() {
  // Initialize pins
  pinMode(PIN_RHB, OUTPUT);
  pinMode(PIN_REN, OUTPUT);
  pinMode(PIN_LHB, OUTPUT);
  pinMode(PIN_LEN, OUTPUT);
  
  pinMode(trigger, OUTPUT);

  // Initialize H-Bridge
  digitalWrite(PIN_RHB, HIGH);
  digitalWrite(PIN_REN, LOW);
  digitalWrite(PIN_LHB, LOW);
  digitalWrite(PIN_LEN, HIGH);

  // Initialize Servo FIRST so it claims its preferred timer (usually Timer 0)
  myServo.setPeriodHertz(50);
  myServo.attach(PIN_SERVO, 500, 2400);

  // Initialize Motor PWM (New Syntax)
  ledcAttach(PIN_REN, 1000, 8);
  ledcWrite(PIN_REN, 0);
}
