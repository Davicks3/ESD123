#include "PulseCounter.h"

PulseCounter::PulseCounter(int gpio, pcnt_unit_t unit)
: gpio(gpio), unit(unit)
{}

bool PulseCounter::begin()
{
    pcnt_config_t cfg = {};
    cfg.pulse_gpio_num = gpio;
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.unit           = unit;
    cfg.channel        = PCNT_CHANNEL_0;

    cfg.pos_mode       = PCNT_COUNT_INC;   // count on rising edges
    cfg.neg_mode       = PCNT_COUNT_DIS;   // ignore falling
    cfg.lctrl_mode     = PCNT_MODE_KEEP;
    cfg.hctrl_mode     = PCNT_MODE_KEEP;

    cfg.counter_l_lim  = INT16_MAX;
    cfg.counter_h_lim  = INT16_MAX;

    if (pcnt_unit_config(&cfg) != ESP_OK) return false;

    pcnt_set_filter_value(unit, 0);
    pcnt_filter_disable(unit);

    pcnt_counter_pause(unit);
    pcnt_counter_clear(unit);
    pcnt_counter_resume(unit);

    return true;
}

int16_t PulseCounter::read()
{
    int16_t val = 0;
    pcnt_get_counter_value(unit, &val);
    return val;
}

void PulseCounter::clear()
{
    pcnt_counter_pause(unit);
    pcnt_counter_clear(unit);
    pcnt_counter_resume(unit);
}
