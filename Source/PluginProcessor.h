#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Model/Layout.h"
#include "Erae/EraeConnection.h"
#include "Erae/EraeRenderer.h"
#include "MIDI/EraeMidiOut.h"
#include "MIDI/MPEAllocator.h"
#include "MIDI/BehaviorEngine.h"
#include "Rendering/WidgetRenderer.h"
#include <map>

namespace erae {

class EraeProcessor : public juce::AudioProcessor,
                      public EraeConnection::Listener {
public:
    EraeProcessor();
    ~EraeProcessor() override;

    // AudioProcessor interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Erae Shape Editor"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    Layout& getLayout() { return layout_; }
    EraeConnection& getConnection() { return connection_; }
    EraeRenderer& getRenderer() { return renderer_; }

    // EraeConnection::Listener
    void fingerEvent(const FingerEvent& event) override;
    void connectionChanged(bool connected) override;

    // Touch positions for UI overlay
    struct FingerInfo { float x, y, z; };
    std::map<uint64_t, FingerInfo> getActiveFingers() const;

    // Per-shape widget state for visual rendering
    std::map<std::string, WidgetState> getShapeWidgetStates() const;

private:
    Layout layout_;
    EraeConnection connection_;
    EraeMidiOut midiOut_;
    MPEAllocator mpeAllocator_;
    BehaviorEngine behaviorEngine_;
    EraeRenderer renderer_;

    // Finger → shape capture (finger stays on shape from DOWN through UP)
    std::map<uint64_t, std::string> fingerShapeMap_;

    // Thread-safe finger position storage for UI
    std::map<uint64_t, FingerInfo> activeFingers_;
    mutable juce::SpinLock fingerLock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EraeProcessor)
};

} // namespace erae
