#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cstdint>
#include <numeric>

namespace erae {

// ============================================================
// Erae II SysEx protocol constants and helpers
// Ported from erae_api_sysex.py + utils.py
// ============================================================
namespace SysEx {

    // SysEx framing
    inline constexpr uint8_t START = 0xF0;
    inline constexpr uint8_t END   = 0xF7;

    // Embodme / Erae II identifiers
    inline const std::vector<uint8_t> MANUFACTURER_ID = {0x00, 0x21, 0x50};
    inline const std::vector<uint8_t> HW_FAMILY       = {0x00, 0x01};
    inline const std::vector<uint8_t> ERAE2_MEMBER    = {0x00, 0x02};
    inline const std::vector<uint8_t> NETWORK_ID      = {0x01};
    inline const std::vector<uint8_t> SERVICE          = {0x01};
    inline const std::vector<uint8_t> API              = {0x04};

    // Our receiver prefix — identifies replies meant for us
    inline const std::vector<uint8_t> RECEIVER_PREFIX = {0x01, 0x02, 0x03};

    // Commands
    inline constexpr uint8_t API_MODE_ENABLE       = 0x01;
    inline constexpr uint8_t API_MODE_DISABLE      = 0x02;
    inline constexpr uint8_t ZONE_BOUNDARY_REQUEST = 0x10;
    inline constexpr uint8_t CLEAR_ZONE            = 0x20;
    inline constexpr uint8_t DRAW_PIXEL            = 0x21;
    inline constexpr uint8_t DRAW_RECTANGLE        = 0x22;
    inline constexpr uint8_t DRAW_IMAGE            = 0x23;
    inline constexpr uint8_t API_VERSION_REQUEST   = 0x7F;

    // Reply identifiers
    inline constexpr uint8_t NON_FINGER            = 0x7F;
    inline constexpr uint8_t ZONE_BOUNDARY_REPLY   = 0x01;
    inline constexpr uint8_t API_VERSION_REPLY     = 0x02;

    // Finger actions
    inline constexpr int ACTION_DOWN = 0;
    inline constexpr int ACTION_MOVE = 1;
    inline constexpr int ACTION_UP   = 2;

    // Image chunking — set to full height to send entire image atomically.
    // The Erae II firmware handles full-frame SysEx (Python reference sends
    // the complete image in one message). Splitting into multiple chunks
    // caused visible flashing from partial frame updates.
    inline constexpr int MAX_IMAGE_ROWS = 24;

    // ============================================================
    // Build the SysEx header for Erae II commands
    // ============================================================
    inline std::vector<uint8_t> header()
    {
        std::vector<uint8_t> h;
        h.insert(h.end(), MANUFACTURER_ID.begin(), MANUFACTURER_ID.end());
        h.insert(h.end(), HW_FAMILY.begin(), HW_FAMILY.end());
        h.insert(h.end(), ERAE2_MEMBER.begin(), ERAE2_MEMBER.end());
        h.insert(h.end(), NETWORK_ID.begin(), NETWORK_ID.end());
        h.insert(h.end(), SERVICE.begin(), SERVICE.end());
        h.insert(h.end(), API.begin(), API.end());
        return h;
    }

    // Wrap payload with SysEx framing + header
    inline juce::MidiMessage buildSysEx(const std::vector<uint8_t>& payload)
    {
        auto hdr = header();
        std::vector<uint8_t> data;
        data.reserve(1 + hdr.size() + payload.size() + 1);
        data.push_back(START);
        data.insert(data.end(), hdr.begin(), hdr.end());
        data.insert(data.end(), payload.begin(), payload.end());
        data.push_back(END);
        return juce::MidiMessage(data.data(), (int)data.size());
    }

    // ============================================================
    // 7-bit encoding/decoding (ported from utils.py)
    // ============================================================
    inline int bitized7size(int length)
    {
        return (length / 7) * 8 + ((length % 7 > 0) ? (1 + length % 7) : 0);
    }

    inline int unbitized7size(int length)
    {
        return (length / 8) * 7 + ((length % 8 > 0) ? (length % 8 - 1) : 0);
    }

    inline std::vector<uint8_t> bitize7chksum(const std::vector<uint8_t>& data, bool appendChecksum = true)
    {
        std::vector<uint8_t> out;
        for (size_t i = 0; i < data.size(); i += 7) {
            uint8_t msb = 0;
            size_t chunk = std::min((size_t)7, data.size() - i);
            for (size_t j = 0; j < chunk; ++j)
                msb |= ((data[i + j] & 0x80) >> (j + 1));
            out.push_back(msb);
            for (size_t j = 0; j < chunk; ++j)
                out.push_back(data[i + j] & 0x7F);
        }
        if (appendChecksum) {
            uint8_t chk = 0;
            for (auto b : out) chk ^= b;
            out.push_back(chk);
        }
        return out;
    }

    inline std::vector<uint8_t> unbitize7chksum(const std::vector<uint8_t>& bitized, int checksumByte = -1)
    {
        int outLen = unbitized7size((int)bitized.size());
        std::vector<uint8_t> out(outLen, 0);
        size_t i = 0, outIdx = 0;
        while (i < bitized.size()) {
            for (int j = 0; j < 7; ++j) {
                if (j + 1 + i < bitized.size() && (int)(outIdx + j) < outLen)
                    out[outIdx + j] = ((bitized[i] << (j + 1)) & 0x80) | bitized[i + j + 1];
            }
            outIdx += 7;
            i += 8;
        }
        return out;
    }

    // ============================================================
    // Command builders
    // ============================================================
    inline juce::MidiMessage enableAPI()
    {
        std::vector<uint8_t> p = {API_MODE_ENABLE};
        p.insert(p.end(), RECEIVER_PREFIX.begin(), RECEIVER_PREFIX.end());
        return buildSysEx(p);
    }

    inline juce::MidiMessage disableAPI()
    {
        return buildSysEx({API_MODE_DISABLE});
    }

    inline juce::MidiMessage zoneBoundaryRequest(uint8_t zone = 0)
    {
        return buildSysEx({ZONE_BOUNDARY_REQUEST, zone});
    }

    inline juce::MidiMessage clearZone(uint8_t zone = 0)
    {
        return buildSysEx({CLEAR_ZONE, zone});
    }

    inline juce::MidiMessage drawPixel(uint8_t zone, uint8_t x, uint8_t y,
                                        uint8_t r, uint8_t g, uint8_t b)
    {
        return buildSysEx({DRAW_PIXEL, zone, x, y, r, g, b});
    }

    inline juce::MidiMessage drawRectangle(uint8_t zone, uint8_t x, uint8_t y,
                                            uint8_t w, uint8_t h,
                                            uint8_t r, uint8_t g, uint8_t b)
    {
        return buildSysEx({DRAW_RECTANGLE, zone, x, y, w, h, r, g, b});
    }

    // Draw image — returns multiple messages (chunked by MAX_IMAGE_ROWS)
    inline std::vector<juce::MidiMessage> drawImage(uint8_t zone, uint8_t x, uint8_t y,
                                                     uint8_t w, uint8_t h,
                                                     const std::vector<uint8_t>& rgbData)
    {
        std::vector<juce::MidiMessage> msgs;
        int row = 0;
        while (row < h) {
            int chunkH = std::min(MAX_IMAGE_ROWS, (int)h - row);
            int start = row * w * 3;
            int end = (row + chunkH) * w * 3;
            std::vector<uint8_t> chunk(rgbData.begin() + start, rgbData.begin() + std::min(end, (int)rgbData.size()));
            auto encoded = bitize7chksum(chunk);

            std::vector<uint8_t> payload = {DRAW_IMAGE, zone, x, (uint8_t)(y + row), w, (uint8_t)chunkH};
            payload.insert(payload.end(), encoded.begin(), encoded.end());
            msgs.push_back(buildSysEx(payload));
            row += chunkH;
        }
        return msgs;
    }

} // namespace SysEx
} // namespace erae
