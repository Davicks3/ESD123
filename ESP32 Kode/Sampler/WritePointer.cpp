

#include "WritePointer.h"
#include <esp_err.h>

// We only want to install the PCNT ISR service once globally.
static bool s_pcnt_isr_service_installed = false;

WritePointer::WritePointer(int PCPin, pcnt_unit_t unit)
: PCPin(PCPin), unit(unit), overflowPages(0)
{}

bool WritePointer::begin()
{
    // Configure PCNT to count rising edges on PCPin.
    pcnt_config_t cfg = {};
    cfg.pulse_gpio_num = PCPin;             // LRCLK input pin
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED; // no control signal
    cfg.unit           = unit;
    cfg.channel        = PCNT_CHANNEL_0;

    cfg.pos_mode       = PCNT_COUNT_INC;    // count on positive edge
    cfg.neg_mode       = PCNT_COUNT_DIS;    // ignore negative edge
    cfg.lctrl_mode     = PCNT_MODE_KEEP;    // control mode not used
    cfg.hctrl_mode     = PCNT_MODE_KEEP;

    // Use 0 .. 32767 as the hardware range.
    // When we hit the high limit, the ISR will bump overflowPages and clear.
    cfg.counter_l_lim  = 0;
    cfg.counter_h_lim  = 32767;

    esp_err_t err = pcnt_unit_config(&cfg);
    if (err != ESP_OK) {
        return false;
    }

    // Optional: disable filter (no debounce) – LRCLK is clean digital.
    pcnt_set_filter_value(unit, 0);
    pcnt_filter_disable(unit);

    // Start from a known state.
    pcnt_counter_pause(unit);
    pcnt_counter_clear(unit);

    // Enable high-limit event so we can extend the counter in software.
    pcnt_event_enable(unit, PCNT_EVT_H_LIM);

    // Install ISR service once.
    if (!s_pcnt_isr_service_installed) {
        err = pcnt_isr_service_install(0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            // ESP_ERR_INVALID_STATE means it was already installed by someone else.
            return false;
        }
        s_pcnt_isr_service_installed = true;
    }

    // Attach our ISR handler for this unit.
    err = pcnt_isr_handler_add(unit, isrHandler, this);
    if (err != ESP_OK) {
        return false;
    }

    // Enable interrupts for this unit.
    pcnt_intr_enable(unit);

    // Start counting.
    pcnt_counter_resume(unit);

    return true;
}

uint64_t WritePointer::get() const
{
    // Read the 16-bit hardware counter.
    int16_t hw = 0;
    pcnt_get_counter_value(unit, &hw);
    uint16_t low = static_cast<uint16_t>(hw);

    // Copy overflowPages once to avoid re-reading volatile.
    uint32_t pages = overflowPages;

    // Each "page" corresponds to 32768 counts (0..32767).
    constexpr uint64_t PAGE_SIZE = 32768ULL;

    uint64_t total = static_cast<uint64_t>(pages) * PAGE_SIZE + low;
    return total;
}

void WritePointer::clear()
{
    // Reset both hardware counter and software overflow pages.
    pcnt_counter_pause(unit);
    pcnt_counter_clear(unit);
    overflowPages = 0;
    pcnt_counter_resume(unit);
}

void IRAM_ATTR WritePointer::isrHandler(void *arg)
{
    auto *self = static_cast<WritePointer *>(arg);
    int u = static_cast<int>(self->unit);

    // Check which unit triggered the interrupt.
    uint32_t intr_status = PCNT.int_st.val;
    if (!(intr_status & BIT(u))) {
        return;
    }

    // Clear interrupt for this unit.
    PCNT.int_clr.val = BIT(u);

    // Read status to see which event occurred.
    uint32_t status = PCNT.status_unit[u].val;

    if (status & PCNT_STATUS_H_LIM_M) {
        // We hit the high limit (32767). Advance page counter and clear.
        self->overflowPages++;
        pcnt_counter_clear(self->unit);
    }

    // We ignore low-limit events because we never count down.
}