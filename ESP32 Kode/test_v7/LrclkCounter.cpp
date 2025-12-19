#include "LrclkCounter.h"

bool LrclkCounter::begin(gpio_num_t lrclkPulsePin)
{
    pin = lrclkPulsePin;
    pinMode(pin, INPUT);

    pcnt_config_t cfg = {};
    cfg.pulse_gpio_num = pin;
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.unit           = unit;
    cfg.channel        = PCNT_CHANNEL_0;
    cfg.pos_mode       = PCNT_COUNT_INC;
    cfg.neg_mode       = PCNT_COUNT_DIS;   // count only rising, or use INC if you want both
    cfg.lctrl_mode     = PCNT_MODE_KEEP;
    cfg.hctrl_mode     = PCNT_MODE_KEEP;
    cfg.counter_h_lim  =  32767;
    cfg.counter_l_lim  = -32768;

    if (pcnt_unit_config(&cfg) != ESP_OK) return false;
    pcnt_counter_pause(unit);
    pcnt_counter_clear(unit);
    pcnt_counter_resume(unit);

    // Initialize lastRaw
    pcnt_get_counter_value(unit, &lastRaw);
    hwCount32 = 0;
    return true;
}

void LrclkCounter::update()
{
    int16_t nowRaw = 0;
    pcnt_get_counter_value(unit, &nowRaw);

    // Signed difference handles wrap as long as |diff| < 32768
    int16_t diff = nowRaw - lastRaw;
    lastRaw = nowRaw;

    hwCount32 += (int32_t)diff;
}