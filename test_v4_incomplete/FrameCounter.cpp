#include "FrameCounter.h"

bool FrameCounter::begin(gpio_num_t pulsePin)
{
    pcnt_config_t cfg = {};
    cfg.pulse_gpio_num = pulsePin;
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.unit           = unit;
    cfg.channel        = PCNT_CHANNEL_0;
    cfg.pos_mode       = PCNT_COUNT_INC;   // count rising edges
    cfg.neg_mode       = PCNT_COUNT_DIS;   // ignore falling edges
    cfg.lctrl_mode     = PCNT_MODE_KEEP;   // control not used
    cfg.hctrl_mode     = PCNT_MODE_KEEP;
    cfg.counter_h_lim  = 32767;
    cfg.counter_l_lim  = 0;

    if (pcnt_unit_config(&cfg) != ESP_OK) {
        return false;
    }

    pcnt_counter_pause(unit);
    pcnt_counter_clear(unit);
    pcnt_counter_resume(unit);

    totalFrames = 0;
    initialized = true;
    return true;
}

void FrameCounter::update()
{
    if (!initialized) return;

    int16_t cnt = 0;
    if (pcnt_get_counter_value(unit, &cnt) == ESP_OK) {
        if (cnt != 0) {
            totalFrames += (int16_t)cnt;
            pcnt_counter_clear(unit);
        }
    }
}

void FrameCounter::reset()
{
    totalFrames = 0;
    if (initialized) {
        pcnt_counter_pause(unit);
        pcnt_counter_clear(unit);
        pcnt_counter_resume(unit);
    }
}