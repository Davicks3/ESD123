// LRCLKCounter.cpp
#include "LRCLKCounter.h"

bool LRCLKCounter::begin(int gpioPin)
{
    pcnt_config_t pcnt_config = {};
    pcnt_config.pulse_gpio_num = gpioPin;
    pcnt_config.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    pcnt_config.channel        = PCNT_CHANNEL_0;
    pcnt_config.unit           = _unit;
    pcnt_config.pos_mode       = PCNT_COUNT_INC;    // count on positive edge
    pcnt_config.neg_mode       = PCNT_COUNT_DIS;    // ignore negative
    pcnt_config.counter_h_lim  = 32767;
    pcnt_config.counter_l_lim  = 0;

    if (pcnt_unit_config(&pcnt_config) != ESP_OK) {
        Serial.println("[LRCLKCounter] pcnt_unit_config failed");
        return false;
    }

    // Disable filter (we are well below 1 MHz)
    pcnt_filter_disable(_unit);

    pcnt_counter_pause(_unit);
    pcnt_counter_clear(_unit);
    pcnt_counter_resume(_unit);

    return true;
}

int16_t LRCLKCounter::readAndClear()
{
    int16_t val = 0;
    pcnt_get_counter_value(_unit, &val);
    pcnt_counter_clear(_unit);
    return val;
}