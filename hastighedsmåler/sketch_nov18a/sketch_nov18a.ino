// ---------- Konfiguration ----------
const int pinWheel1 = 2;
const int pinWheel2 = 4;
const int pulsesPerRevolution1 = 16;
const int pulsesPerRevolution2 = 16;
const float wheelDiameter1_m = 0.12;

const unsigned long minPulseIntervalMicros = 2000;
const unsigned long sampleIntervalMs = 100; // 10 Hz

const int fifoLength = 10;
float fifoBuffer[fifoLength];
int fifoCount = 0;
int fifoHead = 0;

const int bufferLength = 40;
float speedBuffer[bufferLength];
int bufferHead = 0;
int bufferCount = 0;

// ---------- Variabler ----------
volatile unsigned long pulses1 = 0;
volatile unsigned long lastPulse1 = 0;
volatile unsigned long pulses2 = 0;
volatile unsigned long lastPulse2 = 0;

// For tidsbaseret extrapolation
float realSpeeds[2] = {0, 0};
unsigned long lastRealTime = 0; // timestamp for sidste reelle måling

// ---------- Interrupt-funktioner ----------
void IRAM_ATTR onPulse1() {
  unsigned long now = micros();
  if (now - lastPulse1 > minPulseIntervalMicros) {
    pulses1++;
    lastPulse1 = now;
  }
}

void IRAM_ATTR onPulse2() {
  unsigned long now = micros();
  if (now - lastPulse2 > minPulseIntervalMicros) {
    pulses2++;
    lastPulse2 = now;
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(pinWheel1, INPUT);
  pinMode(pinWheel2, INPUT);
  attachInterrupt(digitalPinToInterrupt(pinWheel1), onPulse1, FALLING);
  attachInterrupt(digitalPinToInterrupt(pinWheel2), onPulse2, FALLING);
}

// ---------- Measure hastighed ----------
float measureSpeed() {
  static unsigned long lastCalc = 0;
  unsigned long now = millis();
  float elapsed = (now - lastCalc) / 1000.0;
  if (elapsed == 0) elapsed = 0.001;
  lastCalc = now;

  // Læs pulser
  noInterrupts();
  unsigned long p1 = pulses1; pulses1 = 0;
  unsigned long p2 = pulses2; pulses2 = 0;
  interrupts();

  float speed = 0.0;
  bool newMeasurement = (p1 > 0 || p2 > 0);

  if (newMeasurement) {
    // Beregn hastighed normalt
    float rps1 = (p1 / (float)pulsesPerRevolution1) / elapsed;
    float rps2 = (p2 / (float)pulsesPerRevolution2) / elapsed;
    speed = ((rps1 * 3.14159265 * wheelDiameter1_m) + (rps2 * 3.14159265 * wheelDiameter1_m)) / 2.0;

    // Opdater reelle hastigheder og timestamp
    realSpeeds[0] = realSpeeds[1];
    realSpeeds[1] = speed;
    lastRealTime = now;
  } else {
    // Ingen nye pulser: extrapolér tidsbaseret hvis vi har mindst én reelle måling
    if (realSpeeds[0] > 0 || realSpeeds[1] > 0) {
      float slope = realSpeeds[1] - realSpeeds[0];
      float dt = (now - lastRealTime) / 1000.0; // sekunder siden sidste reelle måling
      speed = realSpeeds[1] + slope * dt / sampleIntervalMs * 0.1; // tidsbaseret lineær extrapolation
      if (speed < 0) speed = 0;
    } else {
      speed = 0; // ingen data at forudsige fra
    }
  }

  return speed;
}

// ---------- Main loop ----------
void loop() {
  static unsigned long lastSampleTime = 0;
  unsigned long now = millis();

  if (now - lastSampleTime >= sampleIntervalMs) {
    lastSampleTime = now;
    float speed = measureSpeed();

    // FIFO de første 10 målinger
    if (fifoCount < fifoLength) {
      fifoBuffer[fifoHead] = speed;
      fifoHead = (fifoHead + 1) % fifoLength;
      fifoCount++;
    } 
    // Hovedbuffer kun efter FIFO er fyldt
    else {
      speedBuffer[bufferHead] = speed;
      bufferHead = (bufferHead + 1) % bufferLength;
      if (bufferCount < bufferLength) bufferCount++;
    }
  }

  // ---------- Serial dump ----------
  if (bufferCount >= bufferLength && Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "dump") {
      Serial.println("Dumping buffer:");

      // Først print 0 for FIFO-målinger hvis nødvendigt
      for (int i = 0; i < fifoLength; i++) {
        Serial.println(0, 3);
      }

      // Print hovedbuffer
      int index = bufferHead;
      for (int i = 0; i < bufferLength; i++) {
        Serial.println(speedBuffer[index], 3);
        index = (index + 1) % bufferLength;
      }

      // Nulstil buffer
      bufferCount = 0;
      bufferHead = 0;
      fifoCount = 0;
      fifoHead = 0;
      realSpeeds[0] = 0;
      realSpeeds[1] = 0;
      lastRealTime = 0;
    }
  }
}
