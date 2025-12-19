#include "Sampler.h"

Sampler::Sampler(int bckPin, int wsPin, int dataPin, int markerPin)
: _bckPin(bckPin),
  _wsPin(wsPin),
  _dataPin(dataPin),
  _markerPin(markerPin),
  _writeIndex(0),
  _triggerIndex(0),
  _triggered(false),
  _markerActive(false),
  _markerEndMs(0)
{}

bool Sampler::begin()
{
    // ---- I2S configuration ----
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE_HZ,
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num   = _bckPin,
        .ws_io_num    = _wsPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = _dataPin
    };

    if (i2s_driver_install(I2S_PORT, &i2s_config, 0, nullptr) != ESP_OK) {
        Serial.println("i2s_driver_install failed");
        return false;
    }
    if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
        Serial.println("i2s_set_pin failed");
        return false;
    }
    if (i2s_set_clk(I2S_PORT, SAMPLE_RATE_HZ, BITS_PER_SAMPLE, I2S_CHANNEL_STEREO) != ESP_OK) {
        Serial.println("i2s_set_clk failed");
        return false;
    }

    // Marker pin
    pinMode(_markerPin, OUTPUT);
    digitalWrite(_markerPin, LOW);

    _writeIndex   = 0;
    _triggerIndex = 0;
    _triggered    = false;

    // ---- ADC settle: discard first 100ms of data ----
    const uint32_t settleFrames = SAMPLE_RATE_HZ / 10; // ~19200 frames
    const uint32_t settleBytes  = settleFrames * BYTES_PER_FRAME;

    uint8_t dummy[256];
    uint32_t discarded = 0;
    while (discarded < settleBytes) {
        size_t toRead = settleBytes - discarded;
        if (toRead > sizeof(dummy)) toRead = sizeof(dummy);

        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_PORT, dummy, toRead, &bytesRead, portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) break;

        discarded += (uint32_t)bytesRead;
    }

    Serial.println("Sampler::begin done (ADC settled)");
    return true;
}

void Sampler::update()
{
    // 1) Turn off marker pulse if its time has passed
    if (_markerActive && (int32_t)(millis() - _markerEndMs) >= 0) {
        digitalWrite(_markerPin, LOW);
        _markerActive = false;
    }

    // 2) Drain I2S into ring buffer
    const size_t MAX_FRAMES_PER_READ = 32;
    uint8_t tempBuf[MAX_FRAMES_PER_READ * BYTES_PER_FRAME];

    while (true) {
        size_t bytesRead = 0;
        esp_err_t err = i2s_read(
            I2S_PORT,
            tempBuf,
            sizeof(tempBuf),
            &bytesRead,
            0  // timeout=0 => non-blocking
        );

        if (err != ESP_OK || bytesRead == 0) {
            // nothing available right now
            break;
        }

        size_t framesRead = bytesRead / BYTES_PER_FRAME;
        if (framesRead == 0) break;

        int32_t *codes = reinterpret_cast<int32_t *>(tempBuf);

        for (size_t f = 0; f < framesRead; ++f) {
            int32_t codeL = codes[f * 2 + 0];
            int32_t codeR = codes[f * 2 + 1];

            uint32_t idx     = _writeIndex;
            uint32_t ringPos = idx % RING_FRAMES;
            _ring[ringPos].L = codeL;
            _ring[ringPos].R = codeR;
            _writeIndex = idx + 1;
        }
    }
}

void Sampler::onTriggerISR()
{
    // Short pulse on marker pin (you'll use that to yank the analog input)
    digitalWrite(_markerPin, HIGH);
    _markerActive = true;
    _markerEndMs  = millis() + 1;  // ~1 ms pulse

    // Snapshot of write index at trigger moment
    _triggerIndex = _writeIndex;
    _triggered    = true;
}

uint32_t Sampler::_indexDiff(uint32_t newer, uint32_t older) const
{
    // uint32_t arithmetic naturally wraps; this works as long as
    // differences are << 2^32, which they are here.
    return newer - older;
}

uint32_t Sampler::_framesAvailableSince(uint32_t index) const
{
    return _indexDiff(_writeIndex, index);
}

uint32_t Sampler::framesSinceTrigger() const
{
    if (!_triggered) return 0;
    return _framesAvailableSince(_triggerIndex);
}

size_t Sampler::copyFromTrigger(float *dest, size_t frames)
{
    if (!_triggered || dest == nullptr || frames == 0) {
        return 0;
    }

    // Make a local snapshot of writeIndex & triggerIndex to avoid race issues
    uint32_t wi  = _writeIndex;
    uint32_t tri = _triggerIndex;

    // How many frames exist after trigger so far?
    uint32_t available = _indexDiff(wi, tri);
    if (available < frames) {
        // Not enough post-trigger frames yet
        return 0;
    }

    // Start copying at triggerIndex (first post-trigger frame can also be tri+0,
    // adjust by +1 if you want "after trigger" instead of "at trigger").
    uint32_t startIndex = tri;

    for (size_t f = 0; f < frames; ++f) {
        uint32_t srcIdx   = startIndex + f;
        uint32_t ringPos  = srcIdx % RING_FRAMES;
        int32_t  codeL    = _ring[ringPos].L;
        int32_t  codeR    = _ring[ringPos].R;

        dest[2 * f + 0] = _codeToVoltage(codeL);
        dest[2 * f + 1] = _codeToVoltage(codeR);
    }

    return frames;
}

float Sampler::_codeToVoltage(int32_t code) const
{
    // PCM1809 full-scale ≈ 2 Vrms differential => 2.828 Vpeak diff
    const float CODE_FS        = 2147483648.0f;        // 2^31
    const float VFS_DIFF_RMS   = 2.0f;
    const float VFS_DIFF_PEAK  = VFS_DIFF_RMS * 1.41421356237f; // ~2.828 V
    return (static_cast<float>(code) / CODE_FS) * VFS_DIFF_PEAK;
}