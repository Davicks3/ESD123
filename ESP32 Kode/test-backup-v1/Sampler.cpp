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

    // --- Step 0: make sure write pointer is up to date ---
    writePtr.update();
    uint64_t writeIndex = writePtr.get();

    Serial.println("===== Sampler::fetch begin =====");
    Serial.print("[fetch] initial readIndex=");
    Serial.print((unsigned long long)readIndex);
    Serial.print(" writeIndex=");
    Serial.print((unsigned long long)writeIndex);
    Serial.print(" triggerIndex=");
    Serial.println((unsigned long long)triggerIndex);

    // Snapshot write pointer BEFORE any discard happens
    uint64_t wp_before_discard = writeIndex;

    // --- Step 1: sync readIndex with writeIndex and ring capacity ---
    syncReadPointer(readIndex, writeIndex, ringFrames);

    Serial.print("[fetch] after sync readIndex=");
    Serial.println((unsigned long long)readIndex);

    // We'll use this only for the data phase
    const size_t MAX_FRAMES_PER_READ = 32;

    // --- Step 2: on first fetch after trigger, discard pre-trigger frames ---
    if (!alignedToTrigger) {
        alignedToTrigger = true;
        Serial.println("[fetch] aligning to trigger...");

        if (triggerIndex > readIndex) {
            uint64_t framesToDiscardRaw = triggerIndex - readIndex;

            Serial.print("[fetch] framesToDiscard (raw) = ");
            Serial.println((unsigned long long)framesToDiscardRaw);

            // If trigger is "older" than what ring can hold, best we can do
            // is discard at most ringFrames from current readIndex.
            uint64_t framesToDiscard = framesToDiscardRaw;
            if (framesToDiscard > (uint64_t)ringFrames) {
                Serial.print("[fetch] WARNING: trigger older than ring. framesToDiscard=");
                Serial.print((unsigned long long)framesToDiscard);
                Serial.print(" ringFrames=");
                Serial.println((unsigned long long)ringFrames);

                framesToDiscard = ringFrames;
            }

            // Single-shot discard: one i2s_read of up to ringFrames
            size_t discardFrames = (size_t)framesToDiscard;
            size_t bytesToRead   = discardFrames * BYTES_PER_FRAME;
            if (bytesToRead > sizeof(discardBuf)) {
                bytesToRead   = sizeof(discardBuf);
                discardFrames = bytesToRead / BYTES_PER_FRAME;
            }

            size_t bytesRead = 0;
            esp_err_t err = i2s_read(I2S_PORT,
                                     discardBuf,
                                     bytesToRead,
                                     &bytesRead,
                                     portMAX_DELAY);
            if (err != ESP_OK || bytesRead == 0) {
                Serial.print("[fetch] discard i2s_read failed, err=");
                Serial.println(err);
            }

            size_t framesActuallyDiscarded = bytesRead / BYTES_PER_FRAME;
            readIndex += framesActuallyDiscarded;

            // Re-snapshot write pointer AFTER discard
            writePtr.update();
            uint64_t wp_after_discard = writePtr.get();

            uint64_t framesProducedDuringDiscard =
                (wp_after_discard >= wp_before_discard)
                    ? (wp_after_discard - wp_before_discard)
                    : 0;

            Serial.print("[fetch] discard done. Requested discard (raw)=");
            Serial.print((unsigned long long)framesToDiscardRaw);
            Serial.print(" cappedTo=");
            Serial.print((unsigned long long)framesToDiscard);
            Serial.print(" actuallyDiscarded=");
            Serial.print((unsigned long long)framesActuallyDiscarded);
            Serial.print(" framesProducedDuringDiscard=");
            Serial.print((unsigned long long)framesProducedDuringDiscard);
            Serial.print(" final readIndex=");
            Serial.println((unsigned long long)readIndex);

            // Update writeIndex for post-trigger read phase
            writeIndex = wp_after_discard;
        } else {
            Serial.println("[fetch] triggerIndex <= readIndex, nothing to discard");
        }
    }

    // --- Step 3: read post-trigger frames into dest ---
    size_t totalFramesOut = 0;
    uint8_t tempBuf[MAX_FRAMES_PER_READ * BYTES_PER_FRAME];

    Serial.print("[fetch] start reading post-trigger frames. framesRequested=");
    Serial.println((unsigned long long)framesRequested);

    while (totalFramesOut < framesRequested) {
        size_t framesToRead = framesRequested - totalFramesOut;
        if (framesToRead > MAX_FRAMES_PER_READ) {
            framesToRead = MAX_FRAMES_PER_READ;
        }

        size_t bytesToRead = framesToRead * BYTES_PER_FRAME;
        size_t bytesRead   = 0;

        esp_err_t err = i2s_read(I2S_PORT,
                                 tempBuf,
                                 bytesToRead,
                                 &bytesRead,
                                 portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) {
            Serial.print("[fetch] data i2s_read failed or zero, err=");
            Serial.println(err);
            break;
        }

        size_t framesRead = bytesRead / BYTES_PER_FRAME;
        if (framesRead == 0) {
            Serial.println("[fetch] data framesRead==0, break");
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
            Serial.println("[fetch] data bytesRead not multiple of BYTES_PER_FRAME, break");
            break;
        }
    }

    Serial.print("[fetch] totalFramesOut=");
    Serial.println((unsigned long long)totalFramesOut);
    Serial.print("===== Sampler::fetch end. final readIndex=");
    Serial.print((unsigned long long)readIndex);
    Serial.println(" =====");

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
