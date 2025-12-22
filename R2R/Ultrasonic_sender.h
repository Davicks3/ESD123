#pragma once
#include <Arduino.h>
#include "driver/pcnt.h"

class UltrasonicSender {
public:
  UltrasonicSender(
      uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
      uint8_t pulsePin,
      uint8_t pcntPin,
      pcnt_unit_t pcntUnit = PCNT_UNIT_0)
    : pulsePin(pulsePin),
      pcntPin(pcntPin),
      pcntUnit(pcntUnit),
      amplitude(0),
      sending(false),
      ledcChannel(255) {

    r2rPins[0] = b0;
    r2rPins[1] = b1;
    r2rPins[2] = b2;
    r2rPins[3] = b3;
  }

  void begin() {
    // R-2R DAC pins
    for (int i = 0; i < 4; i++) {
      pinMode(r2rPins[i], OUTPUT);
      digitalWrite(r2rPins[i], LOW);
    }

    pinMode(pulsePin, OUTPUT);
    digitalWrite(pulsePin, LOW);

    pinMode(pcntPin, INPUT);

    setupPWM();
    setupPCNT();
  }

  void setAmplitude(uint8_t value) {
    amplitude = value & 0x0F;
  }

  void sendPulses(uint16_t cycles) {
    sending = true;

    writeR2R(amplitude);

    pcnt_counter_pause(pcntUnit);
    pcnt_counter_clear(pcntUnit);

    pcnt_set_event_value(pcntUnit, PCNT_EVT_H_LIM, cycles);
    pcnt_event_enable(pcntUnit, PCNT_EVT_H_LIM);

    pcnt_counter_resume(pcntUnit);

    // Start 40 kHz carrier
    ledcWrite(pulsePin, 128);
  }

private:
  uint8_t r2rPins[4];
  uint8_t pulsePin;
  uint8_t pcntPin;
  pcnt_unit_t pcntUnit;

  uint8_t amplitude;
  volatile bool sending;

  uint8_t ledcChannel;

  /* ---------------- PWM ---------------- */

  void setupPWM() {
    // NEW Arduino-ESP32 API (core 3.x)
    ledcAttach(pulsePin, 40000, 8);
    ledcWrite(pulsePin, 0);
  }

  /* ---------------- PCNT ---------------- */

  void setupPCNT() {
    pcnt_config_t cfg = {};
    cfg.pulse_gpio_num = pcntPin;
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.unit           = pcntUnit;
    cfg.channel        = PCNT_CHANNEL_0;

    cfg.pos_mode = PCNT_COUNT_INC;
    cfg.neg_mode = PCNT_COUNT_DIS;
    cfg.lctrl_mode = PCNT_MODE_KEEP;
    cfg.hctrl_mode = PCNT_MODE_KEEP;

    cfg.counter_h_lim = 0;
    cfg.counter_l_lim = 0;

    pcnt_unit_config(&cfg);

    pcnt_counter_pause(pcntUnit);
    pcnt_counter_clear(pcntUnit);

    pcnt_event_enable(pcntUnit, PCNT_EVT_H_LIM);

    pcnt_isr_service_install(0);
    pcnt_isr_handler_add(pcntUnit, pcntISR, this);
  }

  static void IRAM_ATTR pcntISR(void* arg) {
    static_cast<UltrasonicSender*>(arg)->onPCNTLimit();
  }

  void IRAM_ATTR onPCNTLimit() {
    sending = false;
    ledcWrite(pulsePin, 0);
    writeR2R(0);
    pcnt_counter_clear(pcntUnit);
  }

  void writeR2R(uint8_t value) {
    for (int i = 0; i < 4; i++) {
      digitalWrite(r2rPins[i], (value >> i) & 1);
    }
  }
};
