#include "Sampler.h"

Sampler::Sampler(int bclkPin, int lrclkPin, int dataPin)
    : _bclkPin(bclkPin),
      _lrclkPin(lrclkPin),
      _dataPin(dataPin),
      _writeIndex(0),
      _haveTrigger(false),
      _triggerIndex(0),
      _markerThreshold(-0.2f)   // default: -0.2 V
{
}

// Convert raw PCM1809 code (32-bit signed) to differential peak voltage.
// Assumes PCM1809 full scale is 2 Vrms differential => 2.828 Vpeak diff.
float Sampler::codeToVoltage(int32_t code) const
{
    const float CODE_FS       = 2147483648.0f;           // 2^31
    const float VFS_DIFF_RMS  = 2.0f;                    // PCM1809 FS in Vrms (diff)
    const float VFS_DIFF_PEAK = VFS_DIFF_RMS * 1.41421356237f; // ≈2.828 V peak diff
    return (static_cast<float>(code) / CODE_FS) * VFS_DIFF_PEAK;
}

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
        .bck_io_num   = _bclkPin,
        .ws_io_num    = _lrclkPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = _dataPin
    };

    if (i2s_driver_install(I2S_PORT, &i2s_config, 0, nullptr) != ESP_OK) {
        return false;
    }
    if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
        return false;
    }
    if (i2s_set_clk(I2S_PORT, SAMPLE_RATE_HZ, BITS_PER_SAMPLE, I2S_CHANNEL_STEREO) != ESP_OK) {
        return false;
    }

    // Reset state
    _writeIndex   = 0;
    _haveTrigger  = false;
    _triggerIndex = 0;

    // ---- ADC settle: discard first ~100 ms of data ----
    const uint32_t settleFrames = SAMPLE_RATE_HZ / 10; // 100 ms
    const uint32_t settleBytes  = settleFrames * BYTES_PER_FRAME;

    uint8_t  dummy[BYTES_PER_FRAME * 32];  // small chunk
    uint32_t discarded = 0;

    Serial.println("Sampler::begin settling ADC (100ms)...");
    while (discarded < settleBytes) {
        size_t toRead = settleBytes - discarded;
        if (toRead > sizeof(dummy)) {
            toRead = sizeof(dummy);
        }

        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_PORT,
                                 dummy,
                                 toRead,
                                 &bytesRead,
                                 portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) {
            Serial.print("Sampler::begin settle i2s_read err=");
            Serial.println(err);
            break;
        }
        discarded += static_cast<uint32_t>(bytesRead);
    }

    Serial.println("Sampler::begin done (ADC settled)");
    return true;
}

// Pump I2S -> software ring buffer.
// Call this often in loop().
void Sampler::update()
{
    // Read in small non-blocking chunks so we don't hog the CPU.
    uint8_t tempBuf[BYTES_PER_FRAME * 32]; // 32 frames = 256 bytes

    while (true) {
        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_PORT,
                                 tempBuf,
                                 sizeof(tempBuf),
                                 &bytesRead,
                                 0); // timeout=0 -> non-blocking
        if (err != ESP_OK || bytesRead == 0) {
            break;
        }

        size_t framesRead = bytesRead / BYTES_PER_FRAME;
        if (framesRead == 0) {
            break;
        }

        int32_t *codes = reinterpret_cast<int32_t *>(tempBuf);

        for (size_t f = 0; f < framesRead; ++f) {
            int pos = ringPos(_writeIndex);
            _ring[2 * pos + 0] = codes[2 * f + 0]; // L
            _ring[2 * pos + 1] = codes[2 * f + 1]; // R
            _writeIndex++;
        }
    }
}

void Sampler::onTriggerISR()
{
    // Just snapshot where we are in the global frame counter.
    _triggerIndex = _writeIndex;
    _haveTrigger  = true;
}

uint32_t Sampler::framesSinceTrigger() const
{
    if (!_haveTrigger) return 0;

    uint64_t w = _writeIndex;
    if (w <= _triggerIndex) return 0;

    uint64_t diff = w - _triggerIndex;
    if (diff > 0xFFFFFFFFull) diff = 0xFFFFFFFFull;
    return static_cast<uint32_t>(diff);
}

// Search backwards from trigger for last L sample < _markerThreshold
uint64_t Sampler::findLastMarkerBeforeTrigger(uint64_t searchMaxFrames) const
{
    if (!_haveTrigger) {
        return _triggerIndex;
    }

    if (searchMaxFrames > static_cast<uint64_t>(RING_FRAMES)) {
        searchMaxFrames = static_cast<uint64_t>(RING_FRAMES);
    }

    uint64_t start = _triggerIndex;
    if (start == 0) {
        return _triggerIndex;
    }

    uint64_t end = (start > searchMaxFrames) ? (start - searchMaxFrames) : 0;

    bool     found   = false;
    uint64_t bestIdx = _triggerIndex;

    for (uint64_t g = start; g > end; ) {
        g--;
        int pos = ringPos(g);

        int32_t codeL = _ring[2 * pos + 0];
        float   vL    = codeToVoltage(codeL);

        if (vL < _markerThreshold) {
            found   = true;
            bestIdx = g;
            break;  // last marker before trigger
        }
    }

    return found ? bestIdx : _triggerIndex;
}

// Copy framesToCopy frames starting at last marker before trigger into dest as volts.
size_t Sampler::copyFromAlignedMarker(float *dest, size_t framesToCopy)
{
    if (!dest || framesToCopy == 0 || !_haveTrigger) {
        return 0;
    }

    uint64_t markerIndex = findLastMarkerBeforeTrigger(MARKER_SEARCH_MAX_FRAMES);

    uint64_t newestIndexPlusOne = _writeIndex;
    uint64_t availableAfterMarker =
        (newestIndexPlusOne > markerIndex)
            ? (newestIndexPlusOne - markerIndex)
            : 0;

    if (availableAfterMarker < framesToCopy) {
        // Not enough post-marker data in the ring yet.
        return 0;
    }

    for (size_t f = 0; f < framesToCopy; ++f) {
        uint64_t g   = markerIndex + f;
        int      pos = ringPos(g);

        int32_t codeL = _ring[2 * pos + 0];
        int32_t codeR = _ring[2 * pos + 1];

        dest[2 * f + 0] = codeToVoltage(codeL);
        dest[2 * f + 1] = codeToVoltage(codeR);
    }

    return framesToCopy;
}
