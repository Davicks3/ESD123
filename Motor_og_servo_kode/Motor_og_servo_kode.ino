#include <ESP32Servo.h>

int MotorR_EN = 32;    // ← PWM styrepin
int MotorR_CW = 33;
int MotorL_CCW = 25;
int MotorL_EN = 26;

int ServoPin = 27;

Servo myServo;

void setup() {
  pinMode(MotorR_CW, OUTPUT);
  pinMode(MotorL_CCW, OUTPUT);
  pinMode(MotorL_EN, OUTPUT);

  digitalWrite(MotorR_CW, HIGH);
  digitalWrite(MotorL_EN, HIGH);
  digitalWrite(MotorL_CCW, LOW);

  // PWM opsætning på pin 32
  ledcAttachPin(MotorR_EN, 0);   // Kanal 0
  ledcSetup(0, 1000, 8);         // 1 kHz, 8-bit opløsning

  // Servo på pin 27
  myServo.attach(ServoPin, 500, 2400);
}

void loop() {

  // Sweep op (hastighed 15 → 30, servo 0 → 180)
  for (int i = 0; i <= 100; i++) {
    float speed = map(i, 0, 100, 0.00001, 0.0001);
    int angle = map(i, 0, 100, 0, 180);

    ledcWrite(0, speed);     // PWM ud på pin 32
    myServo.write(angle);

    delay(100);
  }

  // Sweep ned (hastighed 30 → 15, servo 180 → 0)
  for (int i = 0; i <= 100; i++) {
    float speed = map(i, 0, 100, 0.0001, 0.00001);
    int angle = map(i, 0, 100, 180, 0);

    ledcWrite(0, speed);
    myServo.write(angle);

    delay(100);
  }
}
