#include "WritePointer.h"

WritePointer::WritePointer(int gpio, pcnt_unit_t unit)
: gpio(gpio),
  unit(unit),
  extCount(0)
{}

bool WritePointer::begin()
{
    pcnt_config_t cfg = {};
    cfg.pulse_gpio_num = gpio;
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.unit           = unit;
    cfg.channel        = PCNT_CHANNEL_0;

    cfg.pos_mode       = PCNT_COUNT_INC;
    cfg.neg_mode       = PCNT_COUNT_DIS;
    cfg.lctrl_mode     = PCNT_MODE_KEEP;
    cfg.hctrl_mode     = PCNT_MODE_KEEP;

    cfg.counter_l_lim  = INT16_MIN;
    cfg.counter_h_lim  = INT16_MAX;

    if (pcnt_unit_config(&cfg) != ESP_OK) {
        return false;
    }

    pcnt_set_filter_value(unit, 0);
    pcnt_filter_disable(unit);

    pcnt_counter_pause(unit);
    pcnt_counter_clear(unit);
    pcnt_counter_resume(unit);

    extCount = 0;
    return true;
}

void WritePointer::update()
{
    int16_t raw = 0;
    pcnt_get_counter_value(unit, &raw);

    // 'raw' = pulses since last clear (as long as we call often enough
    // to avoid saturation). Accumulate into 64-bit counter.
    extCount += (uint64_t)raw;

    // Clear hardware counter for next interval.
    pcnt_counter_pause(unit);
    pcnt_counter_clear(unit);
    pcnt_counter_resume(unit);
}