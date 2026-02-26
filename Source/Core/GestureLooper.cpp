#include "GestureLooper.h"
#include "../PluginProcessor.h"

namespace erae {

GestureLooper::GestureLooper(EraeProcessor& processor)
    : processor_(processor)
{
}

GestureLooper::~GestureLooper()
{
    stopTimer();
}

void GestureLooper::toggleState()
{
    auto s = state_.load(std::memory_order_acquire);
    switch (s) {
        case State::Idle:      startRecording(); break;
        case State::Recording: stopRecording();   break;
        case State::Playing:   stopPlayback();    break;
    }
}

void GestureLooper::stop()
{
    auto s = state_.load(std::memory_order_acquire);
    switch (s) {
        case State::Idle:      break;
        case State::Recording:
            state_.store(State::Idle, std::memory_order_release);
            {
                juce::SpinLock::ScopedLockType lock(recordLock_);
                recording_.clear();
            }
            break;
        case State::Playing:   stopPlayback(); break;
    }
}

void GestureLooper::startRecording()
{
    {
        juce::SpinLock::ScopedLockType lock(recordLock_);
        recording_.clear();
    }
    recordStartTime_ = juce::Time::getMillisecondCounterHiRes();
    state_.store(State::Recording, std::memory_order_release);
    DBG("[looper] Recording started");
}

void GestureLooper::stopRecording()
{
    // Atomically stop capture — MIDI thread will stop appending
    state_.store(State::Idle, std::memory_order_release);
    DBG("[looper] Recording stopped, " + juce::String((int)recording_.size()) + " events");

    finalizeRecording();

    if (recording_.empty()) return;

    loopDurationMs_ = recording_.back().timeMs;
    if (loopDurationMs_ < 50.0) {
        recording_.clear();
        return;
    }

    startPlayback();
}

void GestureLooper::finalizeRecording()
{
    // Inject UP events for any fingers still down at record end
    double endTime = juce::Time::getMillisecondCounterHiRes() - recordStartTime_;

    std::set<uint64_t> activeFingers;
    for (auto& te : recording_) {
        if (te.event.action == SysEx::ACTION_DOWN || te.event.action == SysEx::ACTION_MOVE)
            activeFingers.insert(te.event.fingerId);
        else if (te.event.action == SysEx::ACTION_UP)
            activeFingers.erase(te.event.fingerId);
    }

    for (auto fid : activeFingers) {
        // Find last known position for this finger
        FingerEvent up;
        up.fingerId = fid;
        up.action = SysEx::ACTION_UP;
        up.zoneId = 0;
        up.x = 0; up.y = 0; up.z = 0;

        for (int i = (int)recording_.size() - 1; i >= 0; --i) {
            if (recording_[i].event.fingerId == fid) {
                up.x = recording_[i].event.x;
                up.y = recording_[i].event.y;
                up.zoneId = recording_[i].event.zoneId;
                break;
            }
        }

        recording_.push_back({endTime, up});
    }
}

void GestureLooper::startPlayback()
{
    playbackIndex_ = 0;
    activeReplayFingers_.clear();
    playbackStartTime_ = juce::Time::getMillisecondCounterHiRes();
    state_.store(State::Playing, std::memory_order_release);
    startTimer(1);
    DBG("[looper] Playback started, duration=" + juce::String(loopDurationMs_, 0) + "ms");
}

void GestureLooper::stopPlayback()
{
    stopTimer();
    cleanupReplayedFingers();
    state_.store(State::Idle, std::memory_order_release);
    DBG("[looper] Playback stopped");
}

void GestureLooper::cleanupReplayedFingers()
{
    for (auto fid : activeReplayFingers_) {
        FingerEvent up;
        up.fingerId = fid | REPLAY_ID_BIT;
        up.action = SysEx::ACTION_UP;
        up.zoneId = 0;
        up.x = 0; up.y = 0; up.z = 0;

        // Find last known position
        for (int i = (int)recording_.size() - 1; i >= 0; --i) {
            if (recording_[i].event.fingerId == fid) {
                up.x = recording_[i].event.x;
                up.y = recording_[i].event.y;
                up.zoneId = recording_[i].event.zoneId;
                break;
            }
        }

        processor_.injectReplayEvent(up);
    }
    activeReplayFingers_.clear();
}

void GestureLooper::captureEvent(const FingerEvent& event)
{
    if (state_.load(std::memory_order_acquire) != State::Recording) return;

    double t = juce::Time::getMillisecondCounterHiRes() - recordStartTime_;
    juce::SpinLock::ScopedLockType lock(recordLock_);
    recording_.push_back({t, event});
}

void GestureLooper::timerCallback()
{
    if (state_.load(std::memory_order_acquire) != State::Playing) return;

    double elapsed = juce::Time::getMillisecondCounterHiRes() - playbackStartTime_;

    // Emit all events whose timestamp <= elapsed
    while (playbackIndex_ < recording_.size()) {
        auto& te = recording_[playbackIndex_];
        if (te.timeMs > elapsed) break;

        FingerEvent replay = te.event;
        replay.fingerId |= REPLAY_ID_BIT;

        // Track active replay fingers (by original ID for cleanup)
        if (replay.action == SysEx::ACTION_DOWN || replay.action == SysEx::ACTION_MOVE)
            activeReplayFingers_.insert(te.event.fingerId);
        else if (replay.action == SysEx::ACTION_UP)
            activeReplayFingers_.erase(te.event.fingerId);

        processor_.injectReplayEvent(replay);
        ++playbackIndex_;
    }

    // Loop restart when elapsed >= loopDuration
    if (elapsed >= loopDurationMs_) {
        cleanupReplayedFingers();
        playbackIndex_ = 0;
        playbackStartTime_ = juce::Time::getMillisecondCounterHiRes();
    }
}

double GestureLooper::getPlaybackPosition() const
{
    if (state_.load(std::memory_order_acquire) != State::Playing) return 0.0;
    if (loopDurationMs_ <= 0.0) return 0.0;
    double elapsed = juce::Time::getMillisecondCounterHiRes() - playbackStartTime_;
    return std::min(1.0, elapsed / loopDurationMs_);
}

} // namespace erae
