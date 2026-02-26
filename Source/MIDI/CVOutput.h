#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace erae {

// Lock-free CV output: BehaviorEngine writes channel values,
// processBlock reads them into audio buffer channels.
// Channels are 1V/oct pitch, 0/1 gate, 0-1 continuous, etc.
class CVOutput {
public:
    static constexpr int MaxChannels = 32;

    CVOutput() { clear(); }

    void set(int ch, float value)
    {
        if (ch >= 0 && ch < MaxChannels)
            channels_[ch].store(value, std::memory_order_relaxed);
    }

    float get(int ch) const
    {
        if (ch >= 0 && ch < MaxChannels)
            return channels_[ch].load(std::memory_order_relaxed);
        return 0.0f;
    }

    // Write all CV channel states into the audio buffer as constant-value samples.
    // cvStartChannel is the first audio buffer channel to use (typically 2, after stereo main).
    void writeToBuffer(juce::AudioBuffer<float>& buffer, int cvStartChannel, int numSamples)
    {
        for (int i = 0; i < MaxChannels; ++i) {
            int outCh = cvStartChannel + i;
            if (outCh >= buffer.getNumChannels()) break;
            float val = channels_[i].load(std::memory_order_relaxed);
            auto* dest = buffer.getWritePointer(outCh);
            for (int s = 0; s < numSamples; ++s)
                dest[s] = val;
        }
    }

    void clear()
    {
        for (int i = 0; i < MaxChannels; ++i)
            channels_[i].store(0.0f, std::memory_order_relaxed);
    }

    // Pitch CV helpers (1V/oct standard: C0 = 0V, C1 = 1V, etc.)
    static float noteToPitch(int midiNote) { return (float)midiNote / 12.0f; }
    static float pitchBendToPitch(int midiNote, int pbValue, int pbRange)
    {
        float semitones = (float)midiNote + ((float)(pbValue - 8192) / 8192.0f) * (float)pbRange;
        return semitones / 12.0f;
    }

private:
    std::atomic<float> channels_[MaxChannels];
};

} // namespace erae
