#pragma once
#include <stdint.h>
#include "WritePointer.h"

class Sampler {
    public:
        Sampler(const int BClkPin, const int LRClkPin, const int DataPin)
        : BClkPin(BClkPin), LRClkPin(LRClkPin), DataPin(DataPin), writePtr(14, PCNT_UNIT_0) {}
        
        bool begin();
        void trigger();
        // Fetch frames as voltages (stereo interleaved: L, R, L, R, ...)
        size_t fetch(float* dest, size_t framesRequested);
        
    private:
        const int BClkPin, LRClkPin, DataPin;
        WritePointer writePtr;
        bool triggered = false;
        bool alignedAfterTrigger = false;
        uint64_t triggerFrame = 0;
        uint64_t readIndex = 0;   // software read pointer in global frame space
        int ringFrames = 0;      // total DMA capacity in frames
        // Convert raw PCM1809 code to differential peak voltage
        float codeToVoltage(int32_t code) const;
};