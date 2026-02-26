#include "EraeConnection.h"

namespace erae {

EraeConnection::EraeConnection() {}

EraeConnection::~EraeConnection()
{
    stopTimer();
    disconnect();
}

bool EraeConnection::findEraePorts(PortInfo& outPort, PortInfo& inPort)
{
    auto outDevices = juce::MidiOutput::getAvailableDevices();
    auto inDevices = juce::MidiInput::getAvailableDevices();

    // Output: prefer Lab port for sending API commands
    bool foundOut = false;
    for (auto& dev : outDevices) {
        auto name = dev.name.toLowerCase();
        if (name.contains("lab") && (name.contains("erae") || name.contains("embodme"))) {
            outPort = {dev.identifier, dev.name};
            foundOut = true;
            break;
        }
    }
    if (!foundOut) {
        for (auto& dev : outDevices) {
            auto name = dev.name.toLowerCase();
            if (name.contains("erae") || name.contains("embodme")) {
                outPort = {dev.identifier, dev.name};
                foundOut = true;
                break;
            }
        }
    }

    // Input: prefer Main port (NOT Lab, NOT MPE) for receiving fingerstream
    // Fingerstream SysEx is sent by the Erae on the Main port even though
    // API commands are sent TO the Lab port.
    bool foundIn = false;
    for (auto& dev : inDevices) {
        auto name = dev.name.toLowerCase();
        if ((name.contains("erae") || name.contains("embodme")) &&
            !name.contains("lab") && !name.contains("mpe")) {
            inPort = {dev.identifier, dev.name};
            foundIn = true;
            break;
        }
    }
    if (!foundIn) {
        for (auto& dev : inDevices) {
            auto name = dev.name.toLowerCase();
            if (name.contains("erae") || name.contains("embodme")) {
                inPort = {dev.identifier, dev.name};
                foundIn = true;
                break;
            }
        }
    }

    return foundOut && foundIn;
}

bool EraeConnection::connect()
{
    if (connected_) return true;

    PortInfo outPort, inPort;
    if (!findEraePorts(outPort, inPort)) {
        DBG("[erae] Could not find Erae II MIDI ports");
        return false;
    }

    midiOut_ = juce::MidiOutput::openDevice(outPort.identifier);
    if (!midiOut_) {
        DBG("[erae] Failed to open output: " + outPort.name);
        return false;
    }

    midiIn_ = juce::MidiInput::openDevice(inPort.identifier, this);
    if (!midiIn_) {
        DBG("[erae] Failed to open input: " + inPort.name);
        midiOut_.reset();
        return false;
    }

    midiIn_->start();
    connected_ = true;
    stopTimer();

    DBG("[erae] Connected - OUT: " + outPort.name + " IN: " + inPort.name);

    std::lock_guard<std::mutex> lock(listenerMutex_);
    for (auto* l : listeners_)
        l->connectionChanged(true);

    return true;
}

void EraeConnection::disconnect()
{
    if (!connected_) return;

    disableAPI();

    if (midiIn_) {
        midiIn_->stop();
        midiIn_.reset();
    }
    midiOut_.reset();
    connected_ = false;

    std::lock_guard<std::mutex> lock(listenerMutex_);
    for (auto* l : listeners_)
        l->connectionChanged(false);
}

void EraeConnection::enableAPI()
{
    if (!connected_) return;
    sendMessage(SysEx::enableAPI());
    sendMessage(SysEx::zoneBoundaryRequest(0));
}

void EraeConnection::disableAPI()
{
    if (!connected_) return;
    sendMessage(SysEx::disableAPI());
}

void EraeConnection::sendMessage(const juce::MidiMessage& msg)
{
    if (midiOut_)
        midiOut_->sendMessageNow(msg);
}

void EraeConnection::clearZone(uint8_t zone)
{
    sendMessage(SysEx::clearZone(zone));
}

void EraeConnection::drawRect(uint8_t zone, uint8_t x, uint8_t y,
                               uint8_t w, uint8_t h,
                               uint8_t r, uint8_t g, uint8_t b)
{
    sendMessage(SysEx::drawRectangle(zone, x, y, w, h, r, g, b));
}

void EraeConnection::drawPixel(uint8_t zone, uint8_t x, uint8_t y,
                                uint8_t r, uint8_t g, uint8_t b)
{
    sendMessage(SysEx::drawPixel(zone, x, y, r, g, b));
}

void EraeConnection::drawImage(uint8_t zone, uint8_t x, uint8_t y,
                                uint8_t w, uint8_t h,
                                const std::vector<uint8_t>& rgbData)
{
    auto msgs = SysEx::drawImage(zone, x, y, w, h, rgbData);
    for (auto& msg : msgs)
        sendMessage(msg);
}

void EraeConnection::startAutoConnect(int intervalMs)
{
    if (!connected_)
        startTimer(intervalMs);
}

void EraeConnection::addListener(Listener* l)
{
    std::lock_guard<std::mutex> lock(listenerMutex_);
    listeners_.push_back(l);
}

void EraeConnection::removeListener(Listener* l)
{
    std::lock_guard<std::mutex> lock(listenerMutex_);
    listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), l),
                     listeners_.end());
}

void EraeConnection::handleIncomingMidiMessage(juce::MidiInput*,
                                                const juce::MidiMessage& message)
{
    // Page switch: CC 102-109 on channel 16, value 127 = page entered
    if (message.isController() && message.getChannel() == 16) {
        int cc = message.getControllerNumber();
        if (cc >= 102 && cc <= 109 && message.getControllerValue() == 127) {
            int pageIndex = cc - 102;
            DBG("[erae] Page switch: " + juce::String(pageIndex));
            std::lock_guard<std::mutex> lock(listenerMutex_);
            for (auto* l : listeners_)
                l->pageChangeReceived(pageIndex);
        }
    }

    if (message.isMidiStart() || message.isMidiStop()) {
        bool isStart = message.isMidiStart();
        DBG("[erae] Transport: " + juce::String(isStart ? "Start" : "Stop"));
        std::lock_guard<std::mutex> lock(listenerMutex_);
        for (auto* l : listeners_)
            l->transportReceived(isStart);
    }

    if (!message.isSysEx()) return;

    auto* raw = message.getSysExData();
    int len = message.getSysExDataSize();

    // Check receiver prefix
    int prefixLen = (int)SysEx::RECEIVER_PREFIX.size();
    if (len <= prefixLen) return;
    for (int i = 0; i < prefixLen; ++i)
        if (raw[i] != SysEx::RECEIVER_PREFIX[i]) return;

    int offset = prefixLen;

    if (raw[offset] == SysEx::NON_FINGER) {
        // Control reply
        offset++;
        if (offset < len && raw[offset] == SysEx::ZONE_BOUNDARY_REPLY) {
            offset++;
            if (offset + 2 < len) {
                int zoneId = raw[offset++];
                int w = raw[offset++];
                int h = raw[offset];
                if (w < 0x7F && h < 0x7F) {
                    zoneWidth_ = w;
                    zoneHeight_ = h;
                    DBG("[erae] Zone " + juce::String(zoneId) +
                        ": " + juce::String(w) + "x" + juce::String(h));
                    std::lock_guard<std::mutex> lock(listenerMutex_);
                    for (auto* l : listeners_)
                        l->zoneBoundaryReceived(zoneId, w, h);
                }
            }
        } else if (offset < len && raw[offset] == SysEx::API_VERSION_REPLY) {
            offset++;
            if (offset < len) {
                apiVersion_ = raw[offset];
                DBG("[erae] API version: " + juce::String(apiVersion_));
                std::lock_guard<std::mutex> lock(listenerMutex_);
                for (auto* l : listeners_)
                    l->apiVersionReceived(apiVersion_);
            }
        }
    } else {
        // Fingerstream
        FingerEvent event;
        if (FingerStream::parse(raw + offset, len - offset, event)) {
            std::lock_guard<std::mutex> lock(listenerMutex_);
            for (auto* l : listeners_)
                l->fingerEvent(event);
        }
    }
}

void EraeConnection::timerCallback()
{
    if (!connected_) {
        if (connect()) {
            enableAPI();
        }
    } else {
        stopTimer();
    }
}

} // namespace erae
