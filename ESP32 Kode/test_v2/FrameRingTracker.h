// FrameRingTracker.h
#pragma once
#include <stdint.h>

// Tracks relationship between:
//  - writeIndex        : total frames produced (from LRCLK / PCNT)
//  - hwReadIndex       : frames actually consumed via i2s_read
//  - logicalReadIndex  : "oldest valid frame" index (pushed forward on overrun)
class FrameRingTracker {
public:
    explicit FrameRingTracker(uint32_t ringFrames)
        : _ringFrames(ringFrames),
          _writeIndex(0),
          _hwReadIndex(0),
          _logicalReadIndex(0) {}

    void reset() {
        _writeIndex       = 0;
        _hwReadIndex      = 0;
        _logicalReadIndex = 0;
    }

    // Call whenever LRCLK says "frames new frames produced"
    void onFramesProduced(uint32_t frames) {
        _writeIndex += static_cast<uint64_t>(frames);

        // If writer has lapped logicalReadIndex by more than ringFrames,
        // push logicalReadIndex up — unread data is effectively lost.
        uint64_t maxLag = static_cast<uint64_t>(_ringFrames);
        if (_writeIndex > _logicalReadIndex + maxLag) {
            _logicalReadIndex = _writeIndex - maxLag;
        }
    }

    // Call after each i2s_read with framesRead
    void onFramesRead(uint32_t frames) {
        uint64_t inc = static_cast<uint64_t>(frames);
        _hwReadIndex      += inc;
        _logicalReadIndex += inc;
    }

    uint64_t writeIndex()       const { return _writeIndex; }
    uint64_t hwReadIndex()      const { return _hwReadIndex; }
    uint64_t logicalReadIndex() const { return _logicalReadIndex; }
    uint32_t ringFrames()       const { return _ringFrames; }

    // How many frames do we need to skip with i2s_read to catch hwRead up
    // to logicalReadIndex (i.e., to skip over overrun area)?
    uint32_t framesToSkipOverrun() const {
        if (_hwReadIndex >= _logicalReadIndex) return 0;
        return static_cast<uint32_t>(_logicalReadIndex - _hwReadIndex);
    }

    // After we've skipped overrun, how many frames to discard to get from
    // logicalReadIndex to triggerIndex?
    // Saturates at ringFrames if trigger is too old.
    uint32_t framesToDiscardToTrigger(uint64_t triggerIndex) const {
        if (triggerIndex <= _logicalReadIndex) {
            return 0;
        }

        uint64_t diff = triggerIndex - _logicalReadIndex;
        if (diff > _ringFrames) {
            // Trigger is older than what the ring can hold; best effort:
            return _ringFrames;
        }
        return static_cast<uint32_t>(diff);
    }

private:
    uint32_t _ringFrames;
    uint64_t _writeIndex;
    uint64_t _hwReadIndex;
    uint64_t _logicalReadIndex;
};