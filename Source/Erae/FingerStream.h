#pragma once

#include "EraeSysEx.h"
#include <cstdint>
#include <cstring>
#include <vector>

namespace erae {

struct FingerEvent {
    uint64_t fingerId = 0;
    int zoneId = 0;
    int action = 0; // 0=down, 1=move, 2=up
    float x = 0, y = 0, z = 0;
};

namespace FingerStream {

    // Parse fingerstream from raw SysEx payload (after receiver prefix).
    // Returns true if valid finger event.
    inline bool parse(const uint8_t* data, int length, FingerEvent& out)
    {
        if (length < 2) return false;

        int offset = 0;
        uint8_t actionByte = data[offset++];
        out.zoneId = data[offset++];
        out.action = actionByte & 0x07;

        // Finger ID: 8 bytes → bitized7size(8) = 10 bytes
        int fidLen = SysEx::bitized7size(8);
        if (offset + fidLen > length) return false;
        std::vector<uint8_t> fidBitized(data + offset, data + offset + fidLen);
        offset += fidLen;

        auto fidRaw = SysEx::unbitize7chksum(fidBitized);
        if ((int)fidRaw.size() >= 8) {
            out.fingerId = 0;
            for (int i = 7; i >= 0; --i)
                out.fingerId = (out.fingerId << 8) | fidRaw[i];
        }

        // XYZ: 12 bytes (3 floats) → bitized7size(12) = 14 bytes
        int xyzLen = SysEx::bitized7size(12);
        if (offset + xyzLen > length) return false;
        std::vector<uint8_t> xyzBitized(data + offset, data + offset + xyzLen);
        offset += xyzLen;

        // Checksum byte (Python passes it but we skip validation for now)
        (void)offset;

        auto xyzRaw = SysEx::unbitize7chksum(xyzBitized);
        if ((int)xyzRaw.size() >= 12) {
            std::memcpy(&out.x, xyzRaw.data(), 4);
            std::memcpy(&out.y, xyzRaw.data() + 4, 4);
            std::memcpy(&out.z, xyzRaw.data() + 8, 4);
        }

        return true;
    }

} // namespace FingerStream
} // namespace erae
