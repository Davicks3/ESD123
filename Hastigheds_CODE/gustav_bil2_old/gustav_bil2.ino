#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>

#define LOG_SIZE 50  // RAM-buffer før flush til flash

struct LogEntry {
  uint32_t timeMs;
  float rpmL;
  float rpmR;
  float cmL;
  float cmR;
};

LogEntry logBuffer[LOG_SIZE];
int logIndex = 0;

const char *logFile = "/wheel_log.bin";

// ====== Indstillinger ======
const float pulsesPerRev = 30.0f; // 30 RISING og FALLING pr. rotation (30 CHANGE)
const float cmPerPulse   = 1.173f; // cm distance pr. CHANGE
const unsigned long printEveryMs = 50; //serial.print interval, ændre også antal virtuelle RPM punkter

//    (Interrupt Service Routine)
// ====== ISR-RAM-variabler ======
volatile unsigned long lastPulseL_us = 0;
volatile unsigned long prevPulseL_us = 0;
volatile unsigned long pulseCountL = 0;

volatile unsigned long lastPulseR_us = 0;
volatile unsigned long prevPulseR_us = 0;
volatile unsigned long pulseCountR = 0;

// ====== Estimeret RPM ======
float rpmL_est = 0.0f; //estimeret
float rpmR_est = 0.0f; //estimeret

// ====== ISR-RAM-TYPE ======
// L
void IRAM_ATTR sensorL() {
  unsigned long now = micros();
  prevPulseL_us = lastPulseL_us;
  lastPulseL_us = now;
  pulseCountL++;
}

// R
void IRAM_ATTR sensorR() {
  unsigned long now = micros();
  prevPulseR_us = lastPulseR_us;
  lastPulseR_us = now;
  pulseCountR++;
}





// ====== Virtuel RPM beregning ======
float computeVirtualRPM(unsigned long prevPulse, unsigned long lastPulse, unsigned long now_us) {
  if (prevPulse == 0 || lastPulse == 0) return 0.0f;

  // Interval mellem de to sidste reelle pulser
  float interval_us = (float)(lastPulse - prevPulse);
  if (interval_us <= 0) return 0.0f;

  // Hvis vi har fået en ny puls, brug den normale RPM
  if (now_us <= lastPulse) {
    return (60.0f * 1000000.0f) / (interval_us * pulsesPerRev);
  }

  // Ellers: beregn virtuel RPM baseret på hvor lang tid der er gået siden sidste puls
  float elapsed_us = (float)(now_us - lastPulse);

  // Maksimal realistisk RPM = hvis næste puls kom nu (0.00000001s efter)
  float rpm_virtual = (60.0f * 1000000.0f) / ((elapsed_us + interval_us) * pulsesPerRev);

  // Hvis elapsed_us er meget større end interval_us, falder RPM automatisk
  return rpm_virtual;
}




// ====== RAM Flush til SPIFFS-RAM ======
void flushLogToSPIFFS() {
  File f = SPIFFS.open(logFile, FILE_APPEND);
  if (!f) {
    Serial.println("Fejl: Kan ikke åbne logfil! :(");
    return;
  }
  size_t written = f.write((uint8_t *)logBuffer, sizeof(LogEntry) * logIndex);
  f.close();
  Serial.printf("Flushed %d entries (%d bytes) til SPIFFS\n", logIndex, written);
  logIndex = 0;
  yield(); //Til hunden
}


void markNewSession() {
  File f = SPIFFS.open(logFile, FILE_APPEND);
  if (f) {
    f.println("=== NY SESSION START 222 ==="); // Markeringslinje
    f.close();
  }
}



void setup() {
  Serial.begin(115200);
  delay(500);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS RAM LORRRRT ER SKRALD OG VIRKER IKK!"); //:P
    while (true) { delay(1000); }
  }

  markNewSession();

  pinMode(2, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(2), sensorL, CHANGE);
  attachInterrupt(digitalPinToInterrupt(4), sensorR, CHANGE);

  Serial.println("System start - Virtuel RPM + SPIFFS log");
}





void loop() {
  static unsigned long lastPrintMs = 0;

  unsigned long snap_prevL, snap_lastL, snap_cntL;
  unsigned long snap_prevR, snap_lastR, snap_cntR;

  noInterrupts();
  snap_prevL = prevPulseL_us;
  snap_lastL = lastPulseL_us;
  snap_cntL  = pulseCountL;

  snap_prevR = prevPulseR_us;
  snap_lastR = lastPulseR_us;
  snap_cntR  = pulseCountR;
  interrupts();

  unsigned long now_us = micros();

  // Beregning af virtuel RPM
  rpmL_est = computeVirtualRPM(snap_prevL, snap_lastL, now_us);
  rpmR_est = computeVirtualRPM(snap_prevR, snap_lastR, now_us);

  // Afledt distance
  float cmL = (float)snap_cntL * cmPerPulse;
  float cmR = (float)snap_cntR * cmPerPulse;

  unsigned long now_ms = millis();
  if (now_ms - lastPrintMs >= printEveryMs) {
    lastPrintMs = now_ms;

    Serial.printf("t=%lu ms | rpmL=%.2f rpmR=%.2f | cmL=%.2f cmR=%.2f\n",
                  now_ms, rpmL_est, rpmR_est, cmL, cmR);

    // Log til buffer
    logBuffer[logIndex].timeMs = now_ms;
    logBuffer[logIndex].rpmL   = rpmL_est;
    logBuffer[logIndex].rpmR   = rpmR_est;
    logBuffer[logIndex].cmL    = cmL;
    logBuffer[logIndex].cmR    = cmR;
    logIndex++;

    if (logIndex >= LOG_SIZE) {
      flushLogToSPIFFS();
    }
  }

  // Dump log ved 'd'
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'd') {
      flushLogToSPIFFS();
      File f = SPIFFS.open(logFile, FILE_READ);
      if (!f) {
        Serial.println("Ingen logfil fundet!");
      } else {
        Serial.println("=== LOG DUMP ===");
        LogEntry entry;
        while (f.read((uint8_t *)&entry, sizeof(entry)) == sizeof(entry)) {
          Serial.printf("t=%lu ms | rpmL=%.2f rpmR=%.2f | cmL=%.2f cmR=%.2f\n",
                        entry.timeMs, entry.rpmL, entry.rpmR, entry.cmL, entry.cmR);
          yield();
        }
        f.close();
      }
    } else if (cmd == 'c') {
      SPIFFS.remove(logFile);
      Serial.println("Logfil slettet.");
    }
  }

  delay(1); //Til hundebassen (Watchdog)
}
