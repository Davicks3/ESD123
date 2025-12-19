#pragma once

#include <Arduino.h>
#include "driver/i2s.h"
//#define SAMPLER_DEBUG // To debug or not to debug

#define DMA_BUF_COUNT 30
#define DMA_BUF_LEN 128
#define FRAMES_PER_READ 128
#define FRAMES_PER_SIGNAL 10 * FRAMES_PER_READ // 1152 frames
#define SAFE_FRAME_READ_DIFF 3 * DMA_BUF_LEN

#define SAMPLE_T_US 5.2 //us

#define FLUSH_DMA_BUFFER_THRESHOLD 128

#define SYNC_PULSE_DURATION_US 250 // 48 frames
#define SYNC_PULSE_CODE_LEN 10
const bool SYNC_PULSE_CODE[] = {1, 0, 0, 1, 0, 1, 1, 0, 1, 0}; // {1, 0, 0, 1, 0, 1, 1, 0};
#define SYNC_FRAMES_PER_PULSE 5
#define SYNC_CODE_TOTAL_LEN (SYNC_FRAMES_PER_PULSE * SYNC_PULSE_CODE_LEN)
#define SYNC_PULSE_THRESHOLD 0.5
#define SYNC_SCORE_DIFF_THRESHOLD 5
#define RESYNC_READINDEX_MS 100

static const i2s_port_t I2S_PORT = I2S_NUM_0;

static const int SAMPLE_RATE = 192000;
static const int CHANNELS = 2;
static const i2s_bits_per_sample_t BITS_PER_SAMPLE = I2S_BITS_PER_SAMPLE_32BIT;

static const int BYTES_PER_SAMPLE = (int)BITS_PER_SAMPLE / 8;
static const int BYTES_PER_FRAME  = CHANNELS * BYTES_PER_SAMPLE;
