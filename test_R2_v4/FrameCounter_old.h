/*
// FrameCounter.h
#pragma once
#include <Arduino.h>
#include "driver/pcnt.h"

class FrameCounter {
public:
    bool begin(gpio_num_t lrclkPin) {
        pcnt_config_t cfg = {};
        cfg.pulse_gpio_num = lrclkPin;
        cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
        cfg.channel        = PCNT_CHANNEL_0;
        cfg.unit           = PCNT_UNIT_0;
        cfg.pos_mode       = PCNT_COUNT_INC;
        cfg.neg_mode       = PCNT_COUNT_DIS;
        cfg.lctrl_mode     = PCNT_MODE_KEEP;
        cfg.hctrl_mode     = PCNT_MODE_KEEP;
        cfg.counter_h_lim  = 32767;
        cfg.counter_l_lim  = -32767;

        if (pcnt_unit_config(&cfg) != ESP_OK) { return false; }

        pcnt_counter_pause(PCNT_UNIT_0);
        pcnt_counter_clear(PCNT_UNIT_0);
        pcnt_counter_resume(PCNT_UNIT_0);

        _lastRaw = 0;
        _wraps   = 0;
        _started = true;
        return true;
    }

    int16_t update()
    {
        if (!_started) { return 0; }

        int16_t raw = 0;
        pcnt_get_counter_value(PCNT_UNIT_0, &raw);

        if (raw < _lastRaw) { _wraps++; }

        _lastRaw = raw;
        return raw;
    }

    // Monotonic 64-bit frame index (can run “forever”)
    uint64_t get() {
        if (!_started) { return 0; }

        int16_t raw = update();
        //uint64_t extended = (_wraps << 16) | (uint16_t)raw;
        uint64_t extended = ((uint64_t)_wraps << 16) + (uint16_t)raw;
        return extended;
    }

private:
    bool    _started = false;
    int16_t _lastRaw = 0;
    uint64_t _wraps   = 0;
};
*/
