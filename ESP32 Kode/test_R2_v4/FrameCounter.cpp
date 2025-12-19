// FrameCounter.cpp
#include "FrameCounter.h"
#include "esp_err.h"

FrameCounter::FrameCounter()
    : _pulseGpio(GPIO_NUM_NC),
      _overflowCount(0)
{
}

bool FrameCounter::begin(gpio_num_t pulseGpio)
{
    _pulseGpio = pulseGpio;
    _overflowCount = 0;

    // 1) Configure PCNT unit 0
    pcnt_config_t cfg = {};
    cfg.pulse_gpio_num = _pulseGpio;
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.channel        = PCNT_CHANNEL_0;
    cfg.unit           = UNIT;

    // Count on rising edges only
    cfg.pos_mode = PCNT_COUNT_INC;  // increment on rising edge
    cfg.neg_mode = PCNT_COUNT_DIS;  // ignore falling edge

    // No direction control
    cfg.lctrl_mode = PCNT_MODE_KEEP;
    cfg.hctrl_mode = PCNT_MODE_KEEP;

    cfg.counter_h_lim = HIGH_LIMIT;
    cfg.counter_l_lim = 0;

    esp_err_t err = pcnt_unit_config(&cfg);
    if (err != ESP_OK) {
        return false;
    }

    // 2) Clear and pause before enabling events
    pcnt_counter_pause(UNIT);
    pcnt_counter_clear(UNIT);

    // 3) Enable event on high limit
    pcnt_event_enable(UNIT, PCNT_EVT_H_LIM);

    // 4) Install ISR service (once per app; ignore "already installed" error)
    err = pcnt_isr_service_install(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return false;
    }

    // 5) Register this instance's ISR handler for unit 0
    err = pcnt_isr_handler_add(UNIT, &FrameCounter::isrHandler, this);
    if (err != ESP_OK) {
        return false;
    }

    // 6) Start counting
    pcnt_counter_resume(UNIT);

    return true;
}

void FrameCounter::end()
{
    // Disable high-limit event and stop counter
    pcnt_event_disable(UNIT, PCNT_EVT_H_LIM);
    pcnt_counter_pause(UNIT);

    // We purposely do NOT uninstall the ISR service here (pcnt_isr_service_uninstall)
    // so that other PCNT users in your program are not broken.
}

void FrameCounter::clear()
{
    pcnt_counter_pause(UNIT);
    pcnt_counter_clear(UNIT);
    _overflowCount = 0;
    pcnt_counter_resume(UNIT);
}

uint64_t FrameCounter::get() const
{
    int16_t pcntValue = 0;
    pcnt_get_counter_value(UNIT, &pcntValue);

    // Snapshot overflow counter (non-ISR context; race is acceptable in most apps)
    uint32_t overflowSnapshot = _overflowCount;

    uint64_t perRange = static_cast<uint64_t>(HIGH_LIMIT) + 1ULL;
    uint64_t total = static_cast<uint64_t>(overflowSnapshot) * perRange
                   + static_cast<uint64_t>(pcntValue);

    return total;
}

void IRAM_ATTR FrameCounter::isrHandler(void* arg)
{
    // 'arg' is the 'this' pointer passed in pcnt_isr_handler_add()
    FrameCounter* self = static_cast<FrameCounter*>(arg);
    if (!self) return;

    // We enabled only the high limit event, but we can still check status if we want
    uint32_t status = 0;
    pcnt_get_event_status(UNIT, &status);

    if (status & PCNT_EVT_H_LIM) {
        self->onHighLimitISR();
    }
}

void IRAM_ATTR FrameCounter::onHighLimitISR()
{
    // ISR must be tiny and IRAM-safe: no Serial, no malloc, no heavy C++.
    _overflowCount++;

    // We don't clear the PCNT counter here; it continues counting from HIGH_LIMIT.
    // For many use cases this is enough to extend the range:
    // total = overflow * (HIGH_LIMIT + 1) + current_value.
    //
    // If you want "perfect" modulo behavior, you *could* clear the counter here,
    // but that may or may not be necessary depending on your use case.
}
