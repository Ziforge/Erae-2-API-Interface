#pragma once

#include "../Erae/FingerStream.h"
#include <juce_events/juce_events.h>
#include <atomic>
#include <vector>
#include <set>

namespace erae {

class EraeProcessor;

class GestureLooper : public juce::Timer {
public:
    enum class State { Idle, Recording, Playing };

    static constexpr uint64_t REPLAY_ID_BIT = 0x8000000000000000ULL;

    explicit GestureLooper(EraeProcessor& processor);
    ~GestureLooper() override;

    void toggleState();  // Idle→Rec→Play→Idle
    void stop();         // Any→Idle

    void captureEvent(const FingerEvent& event);
    void timerCallback() override;

    State getState() const { return state_.load(std::memory_order_acquire); }
    double getPlaybackPosition() const;

private:
    struct TimestampedEvent {
        double timeMs;
        FingerEvent event;
    };

    void startRecording();
    void stopRecording();
    void startPlayback();
    void stopPlayback();
    void cleanupReplayedFingers();
    void finalizeRecording();

    EraeProcessor& processor_;
    std::atomic<State> state_ { State::Idle };

    std::vector<TimestampedEvent> recording_;
    juce::SpinLock recordLock_;
    double recordStartTime_ = 0.0;
    double loopDurationMs_ = 0.0;
    double playbackStartTime_ = 0.0;
    size_t playbackIndex_ = 0;
    std::set<uint64_t> activeReplayFingers_;
};

} // namespace erae
