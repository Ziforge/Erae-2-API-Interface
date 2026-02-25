#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Model/Layout.h"
#include "../Model/Behavior.h"
#include <set>
#include <map>
#include <string>

namespace erae {

// Processes incoming MIDI from the DAW and tracks which shapes should be highlighted
class DawFeedback {
public:
    DawFeedback() = default;

    void setEnabled(bool en) { enabled_ = en; }
    bool isEnabled() const { return enabled_; }

    // Build lookup tables from layout — call when layout changes
    void updateFromLayout(const Layout& layout)
    {
        juce::SpinLock::ScopedLockType lock(lock_);
        noteToShape_.clear();

        for (auto& s : layout.shapes()) {
            auto btype = behaviorFromString(s->behavior);
            if (btype == BehaviorType::Trigger || btype == BehaviorType::Momentary || btype == BehaviorType::NotePad) {
                if (auto* obj = s->behaviorParams.getDynamicObject()) {
                    int note = obj->hasProperty("note") ? (int)obj->getProperty("note") : -1;
                    int channel = obj->hasProperty("channel") ? (int)obj->getProperty("channel") : 0;
                    if (note >= 0) {
                        int key = (channel << 8) | note;
                        noteToShape_[key] = s->id;
                    }
                }
            }
        }
    }

    // Process incoming MIDI messages (call from processBlock)
    void processIncomingMidi(const juce::MidiBuffer& buffer)
    {
        if (!enabled_) return;

        juce::SpinLock::ScopedLockType lock(lock_);

        for (auto metadata : buffer) {
            auto msg = metadata.getMessage();
            int channel = msg.getChannel() - 1; // 0-indexed
            int key = (channel << 8);

            if (msg.isNoteOn()) {
                key |= msg.getNoteNumber();
                auto it = noteToShape_.find(key);
                if (it != noteToShape_.end())
                    highlightedShapes_.insert(it->second);
            }
            else if (msg.isNoteOff()) {
                key |= msg.getNoteNumber();
                auto it = noteToShape_.find(key);
                if (it != noteToShape_.end())
                    highlightedShapes_.erase(it->second);
            }
        }
    }

    // Thread-safe read of currently highlighted shape IDs
    std::set<std::string> getHighlightedShapes() const
    {
        juce::SpinLock::ScopedLockType lock(lock_);
        return highlightedShapes_;
    }

    void clear()
    {
        juce::SpinLock::ScopedLockType lock(lock_);
        highlightedShapes_.clear();
    }

private:
    bool enabled_ = false;
    mutable juce::SpinLock lock_;
    std::map<int, std::string> noteToShape_; // (channel<<8 | note) -> shapeId
    std::set<std::string> highlightedShapes_;
};

} // namespace erae
