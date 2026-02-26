#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Model/Preset.h"

namespace erae {

EraeProcessor::EraeProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      behaviorEngine_(midiOut_, mpeAllocator_),
      renderer_(multiLayout_.currentPage(), connection_)
{
    renderer_.setProcessor(this);
    behaviorEngine_.setOscOutput(&oscOutput_);
    behaviorEngine_.setCVOutput(&cvOutput_);

    // Load default drum pads layout
    multiLayout_.currentPage().setShapes(Preset::drumPads());
    dawFeedback_.updateFromLayout(multiLayout_.currentPage());

    // Register for finger events
    connection_.addListener(this);

    // Try to connect to Erae II on startup; auto-retry if not found
    if (connection_.connect())
        connection_.enableAPI();
    else
        connection_.startAutoConnect();
}

EraeProcessor::~EraeProcessor()
{
    behaviorEngine_.allNotesOff();
    connection_.removeListener(this);
    connection_.disconnect();
}

void EraeProcessor::prepareToPlay(double, int) {}
void EraeProcessor::releaseResources() {}

bool EraeProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    int numOut = layouts.getMainOutputChannelSet().size();
    return numOut >= 2 && numOut <= 34; // 2 main + up to 32 CV
}

void EraeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Clear audio buffer (silence main channels)
    buffer.clear();

    // Process incoming MIDI from DAW for feedback highlighting
    dawFeedback_.processIncomingMidi(midiMessages);

    // Drain generated MIDI (from touch → behavior engine) into the output
    midiOut_.drainInto(midiMessages, buffer.getNumSamples());

    // Write CV output to audio channels 2+ (if available)
    if (buffer.getNumChannels() > 2)
        cvOutput_.writeToBuffer(buffer, 2, buffer.getNumSamples());
}

juce::AudioProcessorEditor* EraeProcessor::createEditor()
{
    return new EraeEditor(*this);
}

void EraeProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("layout", multiLayout_.toVar());
    root->setProperty("per_finger_colors", perFingerColors_);
    root->setProperty("daw_feedback", dawFeedback_.isEnabled());
    root->setProperty("osc", oscOutput_.toVar());
    auto json = juce::JSON::toString(juce::var(root));
    destData.append(json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void EraeProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto json = juce::String::fromUTF8(static_cast<const char*>(data), sizeInBytes);
    auto parsed = juce::JSON::parse(json);
    if (!parsed.isObject()) return;

    auto* obj = parsed.getDynamicObject();
    if (!obj) return;

    if (obj->hasProperty("layout")) {
        // New format with settings
        multiLayout_.fromVar(obj->getProperty("layout"));
        perFingerColors_ = (bool)parsed.getProperty("per_finger_colors", true);
        dawFeedback_.setEnabled((bool)parsed.getProperty("daw_feedback", false));
        oscOutput_.fromVar(parsed.getProperty("osc", {}));
    } else {
        // Old format — layout directly (v1/v2 without settings wrapper)
        multiLayout_.fromVar(parsed);
    }

    dawFeedback_.updateFromLayout(multiLayout_.currentPage());
}

void EraeProcessor::fingerEvent(const FingerEvent& event)
{
    // Flip Y: Erae hardware Y=0 is at bottom, software Y=0 is at top
    FingerEvent flipped = event;
    flipped.y = 23.0f - event.y;

    // Update finger positions for UI overlay + fingerShapeMap under same lock
    {
        juce::SpinLock::ScopedLockType lock(fingerLock_);
        if (flipped.action == SysEx::ACTION_UP) {
            activeFingers_.erase(flipped.fingerId);
            fingerShapeMap_.erase(flipped.fingerId);
        } else {
            activeFingers_[flipped.fingerId] = {flipped.x, flipped.y, flipped.z};
        }
    }

    // Hit test: on DOWN capture shape, on MOVE re-test so sliding off clears
    Shape* shape = nullptr;
    if (flipped.action != SysEx::ACTION_UP) {
        shape = getLayout().hitTest(flipped.x, flipped.y);
        juce::SpinLock::ScopedLockType lock(fingerLock_);
        if (shape)
            fingerShapeMap_[flipped.fingerId] = shape->id;
        else
            fingerShapeMap_.erase(flipped.fingerId);
    }

    // Dispatch to behavior engine for MIDI generation
    behaviorEngine_.handle(flipped, shape);

    // Kick renderer for widget animation while fingers are active
    if (shape && visualStyleFromString(shape->visualStyle) != VisualStyle::Static)
        renderer_.requestFullRedraw();
}

void EraeProcessor::connectionChanged(bool connected)
{
    if (connected) {
        // Push layout to Erae surface
        renderer_.requestFullRedraw();
    }
}

std::map<uint64_t, EraeProcessor::FingerInfo> EraeProcessor::getActiveFingers() const
{
    juce::SpinLock::ScopedLockType lock(fingerLock_);
    return activeFingers_;
}

std::map<std::string, WidgetState> EraeProcessor::getShapeWidgetStates() const
{
    std::map<std::string, WidgetState> result;
    juce::SpinLock::ScopedLockType lock(fingerLock_);

    for (auto& [fingerId, shapeId] : fingerShapeMap_) {
        auto fingerIt = activeFingers_.find(fingerId);
        if (fingerIt == activeFingers_.end()) continue;

        auto* shape = multiLayout_.currentPage().getShape(shapeId);
        if (!shape) continue;

        auto bb = shape->bbox();
        float bbW = bb.xMax - bb.xMin;
        float bbH = bb.yMax - bb.yMin;

        float normX = (bbW > 0) ? (fingerIt->second.x - bb.xMin) / bbW : 0.5f;
        float normY = (bbH > 0) ? (fingerIt->second.y - bb.yMin) / bbH : 0.5f;
        normX = std::max(0.0f, std::min(1.0f, normX));
        normY = std::max(0.0f, std::min(1.0f, normY));

        // Use the finger with highest pressure if multiple fingers on same shape
        auto existing = result.find(shapeId);
        if (existing == result.end() || fingerIt->second.z > existing->second.pressure) {
            result[shapeId] = {normX, normY, fingerIt->second.z, true};
        }
    }
    // Merge DAW feedback highlights
    if (dawFeedback_.isEnabled()) {
        auto highlighted = dawFeedback_.getHighlightedShapes();
        for (auto& shapeId : highlighted) {
            auto it = result.find(shapeId);
            if (it == result.end()) {
                // Shape not touched by finger — add a synthetic "active" state
                result[shapeId] = {0.5f, 0.5f, 0.5f, true};
            }
        }
    }

    return result;
}

} // namespace erae

// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new erae::EraeProcessor();
}
