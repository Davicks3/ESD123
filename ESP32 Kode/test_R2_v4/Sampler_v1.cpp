/*
#include "Sampler.h"





bool Sampler::begin()
{
    pinMode(sync_pulse_pin, OUTPUT);
    digitalWrite(sync_pulse_pin, LOW);

    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate          = SAMPLE_RATE;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = DMA_BUF_COUNT;
    cfg.dma_buf_len          = DMA_BUF_LEN;
    cfg.use_apll             = true;
    cfg.tx_desc_auto_clear   = false;
    cfg.fixed_mclk           = 0;

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = bclkPin;
    pins.ws_io_num    = lrclkPin;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num  = dataInPin;

    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) return false;
    if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) return false;
    if (i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO) != ESP_OK) return false;

    if (!frameCounter.begin(GPIO_NUM_14)) {
        return false;
    }

    i2s_start(I2S_PORT);
    return true;
}

void Sampler::handle()
{
    //frameCounter.update(); // Will be called during the read_lag check
    
    static unsigned long last_millis = millis();
    static uint64_t last_index = frameCounter.get();
    unsigned long now = millis();

    if (now - last_millis >= RESYNC_READINDEX_MS) {
      uint64_t index = frameCounter.get();
      Serial.print("LRCLK freq: "); Serial.println((index - last_index) / (now - last_millis));
      last_index = index;
      sync_indicies(); last_millis = now;
    }
    uint64_t read_lag = frameCounter.get() - readIndex;
    if (read_lag >= FLUSH_DMA_BUFFER_THRESHOLD) { discard_frames(read_lag); }
}

void Sampler::trigger()
{
    triggerIndex = frameCounter.get();
    triggered = true;
}

size_t Sampler::fetch(float* l_buf, float* r_buf, uint16_t* offset)
{
    if (!triggered) {return 0; }

    triggered = false;
    
    uint64_t frames_to_discard = triggerIndex - readIndex;
    *offset = 0;

    if (readIndex > triggerIndex)
    {
        frames_to_discard = 0;
        *offset = readIndex - triggerIndex;
    }
    
    discard_frames(frames_to_discard);

    size_t total_frames_read = 0;
    size_t frames_left = FRAMES_PER_SIGNAL;
    while (total_frames_read < FRAMES_PER_SIGNAL)
    {
        size_t frames_read = read_samples(l_buf + total_frames_read, r_buf + total_frames_read);
        if (frames_read == 0) { break; }
        total_frames_read += frames_read;
    }
    return total_frames_read;
}

size_t Sampler::discard_frames(size_t frames_to_discard)
{
    size_t total_frames_discarded = 0;
    static uint8_t dummy[FRAMES_PER_READ * BYTES_PER_FRAME];

    while (total_frames_discarded < frames_to_discard)
    {
        size_t frames_to_read = frames_to_discard - total_frames_discarded;
        if (frames_to_read > FRAMES_PER_READ) { frames_to_read = FRAMES_PER_READ; }
        
        size_t frames_read = read_frames(frames_to_read, dummy);
        if (frames_read == 0) { break; }
        total_frames_discarded += frames_read;
    }
    return total_frames_discarded;
}

bool Sampler::sync_indicies()
{
    // Get index of our sync pulse.
    uint64_t sync_index = frameCounter.get();
    
    float buf[FRAMES_PER_READ];
    float dummy[FRAMES_PER_READ];
    float max_val = -10.0;
    float min_val = 10.0;
    size_t samples_read;

    // Get baseline value
    float baseline = 0.0;
    samples_read = read_samples(buf, dummy);
    if (samples_read < 10) { return false; }
    for (int k = samples_read - 10; k < samples_read; k++)
    {
        baseline += buf[k];
    }
    baseline /= 10;
    

    // Pulse the transistor, to get a negative value to read.
    digitalWrite(sync_pulse_pin, HIGH);
    delayMicroseconds(SYNC_PULSE_DURATION_US);
    digitalWrite(sync_pulse_pin, LOW);

    // Now we look for the pulse, to determine the read index.
    bool found_sync = false;
    size_t total_samples_read = 0;

    #ifdef SAMPLER_DEBUG
    uint64_t old_readIndex = 0;
    #endif

    while (!found_sync && total_samples_read < DMA_BUF_COUNT * DMA_BUF_LEN)
    {
        samples_read = read_samples(buf, dummy);
        total_samples_read += samples_read;

        for (size_t i = 0; i < samples_read; i++)
        {
            max_val = max(max_val, buf[i]);
            min_val = min(min_val, buf[i]);
            if (buf[i] - baseline > -SYNC_PULSE_THRESHOLD) { continue; }
            
            #ifdef SAMPLER_DEBUG
            old_readIndex = readIndex;
            #endif
            uint64_t sampleIndex = readIndex - (samples_read - i);
            int64_t correction = (int64_t)sync_index - (int64_t)sampleIndex;
            readIndex = (uint64_t)((int64_t)readIndex + correction);
            //readIndex = (uint64_t)(samples_read - i) + sync_index;
            found_sync = true;
            break;

        }
    }
    #ifdef SAMPLER_DEBUG
    if (found_sync) {
        Serial.print("[Sampler::sync_indicies] readIndex diff: ");
        Serial.print((signed long)(readIndex - old_readIndex));
    } else {
        Serial.print("[Sampler::sync_indicies] sync pulse NOT found. read frames: ");
        Serial.print(total_samples_read);
    }
    Serial.print(", min: "); Serial.print(min_val, 6);
    Serial.print(", max: "); Serial.print(max_val, 6);
    Serial.print(", base: "); Serial.println(baseline, 6);
    Serial.print("old read index: "); Serial.print(old_readIndex);
    Serial.print(", sync index: "); Serial.println(sync_index);
    #endif
    return found_sync;
}

size_t Sampler::read_samples(float* l_buf, float* r_buf, TickType_t timeoutTicks)
{
    uint8_t frame_buf[FRAMES_PER_READ * BYTES_PER_FRAME];
    size_t frames_read = read_frames(FRAMES_PER_READ, frame_buf, timeoutTicks);
    if (frames_read == 0) { return 0; }
    to_voltage(frames_read, frame_buf, l_buf, r_buf);
    return frames_read;
}

size_t Sampler::read_frames(size_t frames, uint8_t* buf, TickType_t timeoutTicks)
{
    if (frames <= 0) { return 0; }
    int32_t overhead;
    while (true)
    {
        overhead = (int32_t)(frameCounter.get() - readIndex) - (int32_t)frames - SAFE_FRAME_READ_DIFF;
        if (overhead > 0) { break; }
        delayMicroseconds((int)(abs(overhead) * SAMPLE_T_US));
    }
    
    size_t bytesToRead = (size_t)frames * BYTES_PER_FRAME;
    size_t bytesRead   = 0;
    esp_err_t err = i2s_read(I2S_PORT, buf, bytesToRead, &bytesRead, timeoutTicks);
    if (err != ESP_OK || bytesRead == 0) { return 0; }
    size_t frames_read = bytesRead / BYTES_PER_FRAME;
    readIndex += (uint64_t)frames_read;
    return frames_read;
}

void Sampler::discard_initial()
{
    const uint32_t settleFrames = SAMPLE_RATE / 10; // 100 ms

    size_t discarded_frames = discard_frames(settleFrames);

    #ifdef SAMPLER_DEBUG
        Serial.print("[Sampler::discardInitialSettle] discarded_frames=");
        Serial.println(discarded_frames);
    #endif
}

void Sampler::to_voltage(size_t n_frames, uint8_t* input_buf, float* output_l, float* output_r)
{
    if (n_frames == 0) { return; }
    for (size_t j = 0; j < n_frames; j++)
    {
        // Each frame is 8 bytes: 4 left + 4 right
        const uint8_t* p = input_buf + j * BYTES_PER_FRAME;

        // --- Little-endian decode ---
        int32_t sample_l = (int32_t)p[0] | ((int32_t)p[1] << 8) | ((int32_t)p[2] << 16) | ((int32_t)p[3] << 24);
        int32_t sample_r = (int32_t)p[4] | ((int32_t)p[5] << 8) | ((int32_t)p[6] << 16) | ((int32_t)p[7] << 24);
        
        output_l[j] = sample_to_voltage(sample_l);
        output_r[j] = sample_to_voltage(sample_r);
    }
}

float Sampler::sample_to_voltage(int32_t input)
{
    const float CODE_FS       = 2147483648.0f;            // 2^31
    const float VFS_DIFF_RMS  = 2.0f;                     // 2 Vrms differential
    const float VFS_DIFF_PEAK = VFS_DIFF_RMS * 1.41421356237f;
    return (float)input / CODE_FS * VFS_DIFF_PEAK;
}
*/
