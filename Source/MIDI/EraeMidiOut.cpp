#include "EraeMidiOut.h"

namespace erae {

EraeMidiOut::EraeMidiOut() {}
EraeMidiOut::~EraeMidiOut() {}

void EraeMidiOut::addMessage(const juce::MidiMessage& msg)
{
    juce::SpinLock::ScopedLockType lock(bufferLock_);
    pendingBuffer_.addEvent(msg, 0);
}

void EraeMidiOut::noteOn(int channel, int note, int velocity)
{
    addMessage(juce::MidiMessage::noteOn(channel + 1, note, (juce::uint8)velocity));
}

void EraeMidiOut::noteOff(int channel, int note)
{
    addMessage(juce::MidiMessage::noteOff(channel + 1, note));
}

void EraeMidiOut::cc(int channel, int controller, int value)
{
    addMessage(juce::MidiMessage::controllerEvent(channel + 1, controller, value));
}

void EraeMidiOut::pressure(int channel, int value)
{
    addMessage(juce::MidiMessage::channelPressureChange(channel + 1, value));
}

void EraeMidiOut::pitchBend(int channel, int value)
{
    addMessage(juce::MidiMessage::pitchWheel(channel + 1, value));
}

void EraeMidiOut::drainInto(juce::MidiBuffer& buffer, int)
{
    juce::SpinLock::ScopedLockType lock(bufferLock_);
    for (const auto metadata : pendingBuffer_)
        buffer.addEvent(metadata.getMessage(), 0);
    pendingBuffer_.clear();
}

} // namespace erae
