#include "MPEAllocator.h"

namespace erae {

MPEAllocator::MPEAllocator()
{
    for (auto& ch : channels_)
        ch = {};
}

int MPEAllocator::allocate(uint64_t fingerId)
{
    auto it = fingerToChannel_.find(fingerId);
    if (it != fingerToChannel_.end())
        return it->second;

    // Find free channel
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        if (!channels_[i].active) {
            channels_[i].fingerId = fingerId;
            channels_[i].active = true;
            channels_[i].timestamp = counter_++;
            fingerToChannel_[fingerId] = FIRST_CH + i;
            return FIRST_CH + i;
        }
    }

    // All busy — steal oldest
    int oldest = 0;
    for (int i = 1; i < NUM_CHANNELS; ++i)
        if (channels_[i].timestamp < channels_[oldest].timestamp)
            oldest = i;

    fingerToChannel_.erase(channels_[oldest].fingerId);
    channels_[oldest].fingerId = fingerId;
    channels_[oldest].timestamp = counter_++;
    fingerToChannel_[fingerId] = FIRST_CH + oldest;
    return FIRST_CH + oldest;
}

int MPEAllocator::getChannel(uint64_t fingerId) const
{
    auto it = fingerToChannel_.find(fingerId);
    return (it != fingerToChannel_.end()) ? it->second : -1;
}

void MPEAllocator::release(uint64_t fingerId)
{
    auto it = fingerToChannel_.find(fingerId);
    if (it != fingerToChannel_.end()) {
        int idx = it->second - FIRST_CH;
        channels_[idx].active = false;
        fingerToChannel_.erase(it);
    }
}

void MPEAllocator::releaseAll()
{
    for (auto& ch : channels_)
        ch = {};
    fingerToChannel_.clear();
}

} // namespace erae
