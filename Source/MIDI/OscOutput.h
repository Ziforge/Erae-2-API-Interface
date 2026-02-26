#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <cstring>

namespace erae {

// Lightweight OSC message builder + UDP sender
// Thread-safe with SpinLock for enable/host/port changes
class OscOutput {
public:
    OscOutput() = default;
    ~OscOutput() { disable(); }

    void enable(const std::string& host, int port)
    {
        juce::SpinLock::ScopedLockType lock(lock_);
        host_ = host;
        port_ = port;
        socket_ = std::make_unique<juce::DatagramSocket>(false);
        socket_->bindToPort(0); // any local port
        enabled_ = true;
    }

    void disable()
    {
        juce::SpinLock::ScopedLockType lock(lock_);
        enabled_ = false;
        socket_.reset();
    }

    bool isEnabled() const { return enabled_; }
    std::string getHost() const { return host_; }
    int getPort() const { return port_; }

    void noteOn(int channel, int note, int velocity)
    {
        if (!enabled_) return;
        sendOsc("/erae/note/on", channel, note, velocity);
    }

    void noteOff(int channel, int note)
    {
        if (!enabled_) return;
        sendOsc("/erae/note/off", channel, note);
    }

    void cc(int channel, int controller, int value)
    {
        if (!enabled_) return;
        sendOsc("/erae/cc", channel, controller, value);
    }

    void pressure(int channel, int value)
    {
        if (!enabled_) return;
        sendOsc("/erae/pressure", channel, value);
    }

    void pitchBend(int channel, int value)
    {
        if (!enabled_) return;
        sendOsc("/erae/pitchbend", channel, value);
    }

    void effectMPE(int channel, float x, float y, float z)
    {
        if (!enabled_) return;
        std::vector<uint8_t> buf;
        writeString(buf, "/erae/effect/mpe");
        writeString(buf, ",ifff");
        writeInt32(buf, channel);
        writeFloat32(buf, x);
        writeFloat32(buf, y);
        writeFloat32(buf, z);
        send(buf);
    }

    void fingerUpdate(int fingerId, float x, float y, float z, const std::string& shapeId)
    {
        if (!enabled_) return;
        // Build OSC message: /erae/finger ifffs
        std::vector<uint8_t> buf;
        writeString(buf, "/erae/finger");
        writeString(buf, ",ifffs");
        writeInt32(buf, fingerId);
        writeFloat32(buf, x);
        writeFloat32(buf, y);
        writeFloat32(buf, z);
        writeString(buf, shapeId);
        send(buf);
    }

    // Serialization
    juce::var toVar() const
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("osc_enabled", enabled_);
        obj->setProperty("osc_host", juce::String(host_));
        obj->setProperty("osc_port", port_);
        return juce::var(obj);
    }

    void fromVar(const juce::var& v)
    {
        if (!v.isObject()) return;
        bool en = (bool)v.getProperty("osc_enabled", false);
        auto h = v.getProperty("osc_host", "127.0.0.1").toString().toStdString();
        int p = (int)v.getProperty("osc_port", 9000);
        if (en)
            enable(h, p);
        else {
            host_ = h;
            port_ = p;
        }
    }

private:
    // Minimal OSC packet builder (subset for our message types)
    void sendOsc(const char* address, int a1, int a2, int a3 = -999)
    {
        std::vector<uint8_t> buf;
        writeString(buf, address);
        if (a3 == -999) {
            writeString(buf, ",ii");
            writeInt32(buf, a1);
            writeInt32(buf, a2);
        } else {
            writeString(buf, ",iii");
            writeInt32(buf, a1);
            writeInt32(buf, a2);
            writeInt32(buf, a3);
        }
        send(buf);
    }

    void send(const std::vector<uint8_t>& data)
    {
        juce::SpinLock::ScopedLockType lock(lock_);
        if (socket_ && enabled_)
            socket_->write(host_, port_, data.data(), (int)data.size());
    }

    // OSC string: null-terminated, padded to 4-byte boundary
    static void writeString(std::vector<uint8_t>& buf, const std::string& s)
    {
        for (char c : s) buf.push_back((uint8_t)c);
        buf.push_back(0); // null terminator
        while (buf.size() % 4 != 0) buf.push_back(0); // pad
    }

    // OSC int32: big-endian
    static void writeInt32(std::vector<uint8_t>& buf, int32_t val)
    {
        buf.push_back((uint8_t)((val >> 24) & 0xFF));
        buf.push_back((uint8_t)((val >> 16) & 0xFF));
        buf.push_back((uint8_t)((val >> 8) & 0xFF));
        buf.push_back((uint8_t)(val & 0xFF));
    }

    // OSC float32: big-endian
    static void writeFloat32(std::vector<uint8_t>& buf, float val)
    {
        uint32_t bits;
        std::memcpy(&bits, &val, 4);
        buf.push_back((uint8_t)((bits >> 24) & 0xFF));
        buf.push_back((uint8_t)((bits >> 16) & 0xFF));
        buf.push_back((uint8_t)((bits >> 8) & 0xFF));
        buf.push_back((uint8_t)(bits & 0xFF));
    }

    juce::SpinLock lock_;
    std::unique_ptr<juce::DatagramSocket> socket_;
    bool enabled_ = false;
    std::string host_ = "127.0.0.1";
    int port_ = 9000;
};

} // namespace erae
