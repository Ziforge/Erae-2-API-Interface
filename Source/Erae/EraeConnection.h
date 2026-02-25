#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "EraeSysEx.h"
#include "FingerStream.h"
#include <memory>
#include <mutex>
#include <vector>

namespace erae {

class EraeConnection : public juce::MidiInputCallback,
                       public juce::Timer {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void fingerEvent(const FingerEvent& event) {}
        virtual void zoneBoundaryReceived(int zoneId, int width, int height) {}
        virtual void apiVersionReceived(int version) {}
        virtual void connectionChanged(bool connected) {}
    };

    EraeConnection();
    ~EraeConnection() override;

    bool connect();
    void disconnect();
    bool isConnected() const { return connected_; }

    void enableAPI();
    void disableAPI();
    void sendMessage(const juce::MidiMessage& msg);

    void clearZone(uint8_t zone = 0);
    void drawRect(uint8_t zone, uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                  uint8_t r, uint8_t g, uint8_t b);
    void drawPixel(uint8_t zone, uint8_t x, uint8_t y,
                   uint8_t r, uint8_t g, uint8_t b);
    void drawImage(uint8_t zone, uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                   const std::vector<uint8_t>& rgbData);

    void startAutoConnect(int intervalMs = 3000);

    void addListener(Listener* l);
    void removeListener(Listener* l);

    int getZoneWidth() const { return zoneWidth_; }
    int getZoneHeight() const { return zoneHeight_; }

    // juce::MidiInputCallback
    void handleIncomingMidiMessage(juce::MidiInput* source,
                                   const juce::MidiMessage& message) override;

    // juce::Timer (auto-reconnect)
    void timerCallback() override;

private:
    struct PortInfo {
        juce::String identifier;
        juce::String name;
    };

    bool findEraePorts(PortInfo& outPort, PortInfo& inPort);

    std::unique_ptr<juce::MidiOutput> midiOut_;
    std::unique_ptr<juce::MidiInput> midiIn_;
    bool connected_ = false;
    int zoneWidth_ = 42;
    int zoneHeight_ = 24;
    int apiVersion_ = -1;

    std::vector<Listener*> listeners_;
    std::mutex listenerMutex_;
};

} // namespace erae
