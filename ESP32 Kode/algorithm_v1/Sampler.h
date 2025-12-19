#include "Sampler_settings.h"
#include "FrameCounter.h"

class Sampler {
    public:
    Sampler(const int bclkPin, const int lrclkPin, const int dataInPin, const int sync_pulse_pin)
    : bclkPin(bclkPin), lrclkPin(lrclkPin), dataInPin(dataInPin), sync_pulse_pin(sync_pulse_pin) {}
    bool begin();
    void handle();
    void trigger();
    size_t fetch(float* l_buf, float* r_buf, uint16_t* offset, bool discard_first=false);
    void discard_initial();
    bool get_triggered_state() {return triggered; }

    unsigned long last_resync_millis = 0;

    size_t discard_frames(size_t frames_to_discard);
    bool find_sync_pulse(size_t n_samples, float* buf, uint64_t sync_index, float baseline);
    void send_sync_pulse();
    bool sync_indicies();
    size_t read_samples(float* l_buf, float* r_buf, TickType_t timeoutTicks=portMAX_DELAY);
    size_t read_frames(size_t frames, uint8_t* buf, TickType_t timeoutTicks=portMAX_DELAY);
    void to_voltage(size_t n_frames, uint8_t* input_buf, float* output_l, float* output_r);
    float  sample_to_voltage(int32_t input);

    FrameCounter frameCounter;
    uint64_t writeIndex = 0;
    uint64_t readIndex = 0;
    uint64_t triggerIndex = 0;
    bool triggered = false;
    const int bclkPin, lrclkPin, dataInPin, sync_pulse_pin;

};
