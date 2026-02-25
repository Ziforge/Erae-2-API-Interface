#pragma once

#include <cstdint>
#include <map>

namespace erae {

// MPE lower-zone allocator: MIDI channels 2-16 (indices 1-15), oldest-steal
class MPEAllocator {
public:
    MPEAllocator();

    int allocate(uint64_t fingerId);
    int getChannel(uint64_t fingerId) const;
    void release(uint64_t fingerId);
    void releaseAll();

private:
    static constexpr int FIRST_CH = 1;
    static constexpr int LAST_CH = 15;
    static constexpr int NUM_CHANNELS = LAST_CH - FIRST_CH + 1; // 15

    struct ChannelInfo {
        uint64_t fingerId = 0;
        bool active = false;
        uint64_t timestamp = 0;
    };

    ChannelInfo channels_[NUM_CHANNELS];
    std::map<uint64_t, int> fingerToChannel_;
    uint64_t counter_ = 0;
};

} // namespace erae
