#include "Sampler.h"


static uint8_t discardBuf[1024 * BYTES_PER_FRAME];

// Helper: ensure readIndex is not more than ringFrames behind writeIndex
static void syncReadPointer(uint64_t &readIndex,
                            uint64_t  writeIndex,
                            uint32_t  ringFrames)
{
    if (writeIndex > readIndex + (uint64_t)ringFrames) {
        uint64_t oldRead = readIndex;
        readIndex = writeIndex - (uint64_t)ringFrames;
        Serial.print("[syncReadPointer] reader was too far behind. "
                     "old readIndex=");
        Serial.print(oldRead);
        Serial.print(" new readIndex=");
        Serial.println(readIndex);
    }
}

Sampler::Sampler(int BClkPin, int LRClkPin, int DataPin, WritePointer &writePtrRef)
: bclkPin(BClkPin),
  lrclkPin(LRClkPin),
  dataPin(DataPin),
  writePtr(writePtrRef),
  ringFrames(0),
  readIndex(0),
  triggerIndex(0),
  triggered(false),
  alignedToTrigger(false)
{}

bool Sampler::begin()
{
    Serial.println("[Sampler::begin] start");

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
        .bck_io_num   = bclkPin,
        .ws_io_num    = lrclkPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = dataPin
    };

    if (i2s_driver_install(I2S_PORT, &i2s_config, 0, nullptr) != ESP_OK) {
        Serial.println("[Sampler::begin] i2s_driver_install FAILED");
        return false;
    }
    if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
        Serial.println("[Sampler::begin] i2s_set_pin FAILED");
        return false;
    }
    if (i2s_set_clk(I2S_PORT, SAMPLE_RATE_HZ, BITS_PER_SAMPLE, I2S_CHANNEL_STEREO) != ESP_OK) {
        Serial.println("[Sampler::begin] i2s_set_clk FAILED");
        return false;
    }

    ringFrames       = DMA_BUF_LEN * DMA_BUF_COUNT;
    readIndex        = 0;
    triggered        = false;
    alignedToTrigger = false;
    triggerIndex     = 0;

    Serial.print("[Sampler::begin] ringFrames=");
    Serial.println(ringFrames);

    // ---- ADC settle: discard first ~100 ms of samples ----
    const uint32_t settleFrames = SAMPLE_RATE_HZ / 10;     // 100 ms
    const uint32_t settleBytes  = settleFrames * BYTES_PER_FRAME;

    uint8_t  dummy[256];
    uint32_t discarded = 0;
    while (discarded < settleBytes) {
        size_t toRead = settleBytes - discarded;
        if (toRead > sizeof(dummy)) {
            toRead = sizeof(dummy);
        }

        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_PORT, dummy, toRead, &bytesRead, portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) {
            Serial.println("[Sampler::begin] settle i2s_read failed or 0 bytes");
            break;
        }

        discarded += (uint32_t)bytesRead;
        uint32_t framesRead = bytesRead / BYTES_PER_FRAME;
        readIndex += framesRead;
        
        writePtr.update();
    }

    // At this point, WritePointer is being updated from loop()
    // so writePtr.get() should already reflect all LRCLK edges
    // that have happened during settling.
    uint64_t writeIndex = writePtr.get();
    Serial.print("[Sampler::begin] after settle: writeIndex=");
    Serial.print(writeIndex);
    Serial.print(" readIndex=");
    Serial.println(readIndex);

    // Align so we start from "no unread data"
    readIndex = writeIndex;

    Serial.print("[Sampler::begin] aligned readIndex=");
    Serial.println(readIndex);

    Serial.println("[Sampler::begin] done");
    return true;
}

void Sampler::trigger()
{
    // Snapshot writeIndex in global frame space.
    triggerIndex      = writePtr.get();
    triggered         = true;
    alignedToTrigger  = false;
}

size_t Sampler::fetch(float *dest, size_t framesRequested)
{
    if (!triggered || dest == nullptr || framesRequested == 0) {
        return 0;
    }

    // ---- discard stats ----
    uint64_t framesDiscardRaw    = 0;  // triggerIndex - readIndex (before any capping)
    uint64_t framesDiscardCapped = 0;  // after capping to ringFrames
    uint64_t framesDiscarded     = 0;  // actually discarded by i2s_read

    // ---- write pointer stats ----
    writePtr.update();
    uint64_t wp_before_discard = writePtr.get();
    uint64_t wp_after_discard  = wp_before_discard;
    uint64_t wp_after_fetch    = wp_before_discard;

    // --- Step 1: sync readIndex with writeIndex and ring capacity ---
    uint64_t writeIndex = wp_before_discard;
    syncReadPointer(readIndex, writeIndex, ringFrames);

    // --- Step 2: on first fetch after trigger, discard pre-trigger frames in ONE big read ---
    if (!alignedToTrigger) {
        alignedToTrigger = true;

        if (triggerIndex > readIndex) {
            // Raw distance between where we *can* read and the trigger
            framesDiscardRaw = triggerIndex - readIndex;

            // Cap distance to ringFrames (cannot have more valid history than this)
            uint64_t framesToDiscard = framesDiscardRaw;
            if (framesToDiscard > (uint64_t)ringFrames) {
                framesToDiscard = (uint64_t)ringFrames;
            }
            framesDiscardCapped = framesToDiscard;

            // We'll do a single i2s_read for exactly this many frames (if possible)
            const size_t MAX_DISCARD_FRAMES = 2048; // must be >= ringFrames
            size_t discardFrames = (size_t)framesToDiscard;
            if (discardFrames > MAX_DISCARD_FRAMES) {
                discardFrames = MAX_DISCARD_FRAMES;
            }

            static uint8_t discardBuf[MAX_DISCARD_FRAMES * BYTES_PER_FRAME];

            size_t bytesToDiscard = discardFrames * BYTES_PER_FRAME;
            size_t bytesRead      = 0;

            esp_err_t err = i2s_read(
                I2S_PORT,
                discardBuf,
                bytesToDiscard,
                &bytesRead,
                portMAX_DELAY
            );

            size_t framesRead = 0;
            if (err == ESP_OK && bytesRead > 0) {
                framesRead = bytesRead / BYTES_PER_FRAME;
            }

            // Advance logical readIndex by however many frames we actually threw away
            readIndex         += framesRead;
            framesDiscarded    = framesRead;
        }

        // Latch write pointer *after* discard
        writePtr.update();
        wp_after_discard = writePtr.get();

    } else {
        // Already aligned on a previous fetch; no discard this time
        writePtr.update();
        wp_after_discard = writePtr.get();
    }

    // --- Step 3: read post-trigger frames into dest ---
    size_t totalFramesOut = 0;
    const size_t MAX_FRAMES_PER_READ = 32;
    uint8_t tempBuf[MAX_FRAMES_PER_READ * BYTES_PER_FRAME];

    while (totalFramesOut < framesRequested) {
        size_t framesToRead = framesRequested - totalFramesOut;
        if (framesToRead > MAX_FRAMES_PER_READ) {
            framesToRead = MAX_FRAMES_PER_READ;
        }

        size_t bytesToRead = framesToRead * BYTES_PER_FRAME;
        size_t bytesRead   = 0;

        esp_err_t err = i2s_read(
            I2S_PORT,
            tempBuf,
            bytesToRead,
            &bytesRead,
            portMAX_DELAY
        );
        if (err != ESP_OK || bytesRead == 0) {
            break;
        }

        size_t framesRead = bytesRead / BYTES_PER_FRAME;
        if (framesRead == 0) {
            break;
        }

        int32_t *codes = reinterpret_cast<int32_t *>(tempBuf);

        for (size_t f = 0; f < framesRead; ++f) {
            int32_t codeL = codes[f * 2 + 0];
            int32_t codeR = codes[f * 2 + 1];

            dest[(totalFramesOut + f) * 2 + 0] = codeToVoltage(codeL);
            dest[(totalFramesOut + f) * 2 + 1] = codeToVoltage(codeR);
        }

        totalFramesOut += framesRead;
        readIndex      += framesRead;

        if ((bytesRead % BYTES_PER_FRAME) != 0) {
            break;
        }
    }

    // ---- Latch write pointer after post-trigger read ----
    writePtr.update();
    wp_after_fetch = writePtr.get();

    // ---- Final stats (only after all critical work is done) ----
    uint32_t dW_discard = (uint32_t)(wp_after_discard - wp_before_discard);
    uint32_t dW_fetch   = (uint32_t)(wp_after_fetch   - wp_after_discard);

    Serial.print("[stats] discardRaw=");
    Serial.print((unsigned long long)framesDiscardRaw);
    Serial.print(" capped=");
    Serial.print((unsigned long long)framesDiscardCapped);
    Serial.print(" discarded=");
    Serial.print((unsigned long long)framesDiscarded);
    Serial.print("  dW_discard=");
    Serial.print(dW_discard);
    Serial.print("  dW_fetch=");
    Serial.println(dW_fetch);

    return totalFramesOut;
}

// Convert raw PCM1809 code (32-bit signed) to differential peak voltage.
// Assumes PCM1809 full scale is 2 Vrms differential => ~2.828 Vpeak diff.
float Sampler::codeToVoltage(int32_t code) const
{
    const float CODE_FS       = 2147483648.0f;                 // 2^31
    const float VFS_DIFF_RMS  = 2.0f;
    const float VFS_DIFF_PEAK = VFS_DIFF_RMS * 1.41421356237f; // ~2.828
    return (static_cast<float>(code) / CODE_FS) * VFS_DIFF_PEAK;
}
