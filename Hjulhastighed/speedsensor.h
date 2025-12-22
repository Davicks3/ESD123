#include <Arduino.h>

class PulseSpeed {
public:
  struct Data {
    uint32_t timeMs;
    float rpmL;
    float rpmR;
    float cmL;
    float cmR;
  };

  PulseSpeed(uint8_t pinL,
             uint8_t pinR,
             float pulsesPerRev,
             float cmPerPulse,
             int interruptMode = CHANGE)
  : _pinL(pinL),
    _pinR(pinR),
    _pulsesPerRev(pulsesPerRev),
    _cmPerPulse(cmPerPulse),
    _mode(interruptMode) {}

  void begin() {
    _instance = this;

    pinMode(_pinL, INPUT_PULLUP);
    pinMode(_pinR, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(_pinL), isrL, _mode);
    attachInterrupt(digitalPinToInterrupt(_pinR), isrR, _mode);
  }

  Data read() const {
    uint32_t pL_prev, pL_last, cntL;
    uint32_t pR_prev, pR_last, cntR;

    noInterrupts();
    pL_prev = _prevPulseL_us;
    pL_last = _lastPulseL_us;
    cntL    = _pulseCountL;

    pR_prev = _prevPulseR_us;
    pR_last = _lastPulseR_us;
    cntR    = _pulseCountR;
    interrupts();

    uint32_t now_us = (uint32_t)micros();

    Data d;
    d.timeMs = millis();
    d.rpmL   = computeVirtualRPM(pL_prev, pL_last, now_us, _pulsesPerRev);
    d.rpmR   = computeVirtualRPM(pR_prev, pR_last, now_us, _pulsesPerRev);
    d.cmL    = (float)cntL * _cmPerPulse;
    d.cmR    = (float)cntR * _cmPerPulse;
    return d;
  }

  void resetDistance() {
    noInterrupts();
    _pulseCountL = 0;
    _pulseCountR = 0;
    interrupts();
  }

private:
  static inline float computeVirtualRPM(uint32_t prev,
                                        uint32_t last,
                                        uint32_t now_us,
                                        float pulsesPerRev) {
    if (prev == 0 || last == 0) return 0.0f;

    float interval_us = (float)(last - prev);
    if (interval_us <= 0.0f) return 0.0f;

    if (now_us <= last) {
      return (60.0f * 1e6f) / (interval_us * pulsesPerRev);
    }

    float elapsed_us = (float)(now_us - last);
    return (60.0f * 1e6f) / ((elapsed_us + interval_us) * pulsesPerRev);
  }

  static void IRAM_ATTR isrL() {
    if (!_instance) return;
    uint32_t now = (uint32_t)micros();
    _instance->_prevPulseL_us = _instance->_lastPulseL_us;
    _instance->_lastPulseL_us = now;
    _instance->_pulseCountL++;
  }

  static void IRAM_ATTR isrR() {
    if (!_instance) return;
    uint32_t now = (uint32_t)micros();
    _instance->_prevPulseR_us = _instance->_lastPulseR_us;
    _instance->_lastPulseR_us = now;
    _instance->_pulseCountR++;
  }

  uint8_t _pinL, _pinR;
  float _pulsesPerRev;
  float _cmPerPulse;
  int _mode;

  volatile uint32_t _lastPulseL_us = 0;
  volatile uint32_t _prevPulseL_us = 0;
  volatile uint32_t _pulseCountL   = 0;

  volatile uint32_t _lastPulseR_us = 0;
  volatile uint32_t _prevPulseR_us = 0;
  volatile uint32_t _pulseCountR   = 0;

  static PulseSpeed* _instance;
};