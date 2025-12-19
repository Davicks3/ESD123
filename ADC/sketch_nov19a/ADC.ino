// ESP32 + PCM1809 I2S 32-bit input (Arduino IDE version)
// Samples *exactly 1 ms* of stereo audio ONCE and then stops

#include <Arduino.h>
#include <driver/i2s.h>

#define I2S_PORT        I2S_NUM_0
#define SAMPLE_RATE     192000

// Pin configuration (tilpas til din opsætning)
#define PIN_I2S_BCK     26
#define PIN_I2S_WS      25
#define PIN_I2S_DATA    33

bool done = false;   // sørger for vi kun sampler én gang

void setupI2S() {
    i2s_config_t config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = PIN_I2S_BCK,
        .ws_io_num = PIN_I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_DATA
    };

    i2s_driver_install(I2S_PORT, &config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("I2S PCM1809 32-bit 1 ms capture...");
    setupI2S();
}

void loop() {
    if (done) return;   // stop efter første optagelse

    static int32_t samples_buffer[48 * 2];  // 1 ms @ 48 kHz stereo
    size_t bytes_read = 0;

    // Læs præcis 1 ms (384 bytes) fra I2S
    i2s_read(I2S_PORT, samples_buffer, sizeof(samples_buffer), &bytes_read, portMAX_DELAY);

    int frames = bytes_read / (2 * sizeof(int32_t));  // stereo frames

    Serial.println("1ms samples (L/R):");
    for (int i = 0; i < frames; i++) {
        int32_t L = samples_buffer[i * 2];
        int32_t R = samples_buffer[i * 2 + 1];
        Serial.printf("L: %ld  R: %ld\n", L, R);
    }

    Serial.println("Done. System will not sample again.");
    done = true;
}