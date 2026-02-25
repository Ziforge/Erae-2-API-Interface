#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <memory>

namespace erae {

class EraeMidiOut {
public:
    EraeMidiOut();
    ~EraeMidiOut();

    void noteOn(int channel, int note, int velocity);
    void noteOff(int channel, int note);
    void cc(int channel, int controller, int value);
    void pressure(int channel, int value);
    void pitchBend(int channel, int value); // 0-16383, center=8192

    // VST3/Standalone: drain pending messages into processBlock's MidiBuffer
    void drainInto(juce::MidiBuffer& buffer, int numSamples);

private:
    void addMessage(const juce::MidiMessage& msg);

    juce::MidiBuffer pendingBuffer_;
    juce::SpinLock bufferLock_;
};

} // namespace erae
