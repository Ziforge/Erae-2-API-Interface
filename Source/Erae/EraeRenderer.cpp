#include "EraeRenderer.h"
#include "../PluginProcessor.h"
#include "../Rendering/FingerPalette.h"
#include <cstring>
#include <algorithm>

namespace erae {

EraeRenderer::EraeRenderer(Layout& layout, EraeConnection& connection)
    : layout_(layout), connection_(connection)
{
    layout_.addListener(this);
}

EraeRenderer::~EraeRenderer()
{
    stopTimer();
    layout_.removeListener(this);
}

void EraeRenderer::requestFullRedraw()
{
    dirty_ = true;
    forceFullFrame_ = true;
    if (!isTimerRunning())
        startTimer(50); // 20 fps max
}

void EraeRenderer::layoutChanged()
{
    requestFullRedraw();
}

void EraeRenderer::timerCallback()
{
    if (!connection_.isConnected()) {
        if (!dirty_)
            stopTimer();
        return;
    }

    // Get current widget states from processor
    std::map<std::string, WidgetState> widgetStates;
    if (processor_)
        widgetStates = processor_->getShapeWidgetStates();

    bool widgetsChanged = widgetStates != lastWidgetStates_;

    if (dirty_ || widgetsChanged) {
        // Build a 42×24 framebuffer, composite all shapes, send as one image
        static constexpr int W = 42, H = 24;
        uint8_t fb[H][W][3];  // [y][x][rgb] — 7-bit values
        std::memset(fb, 0, sizeof(fb));

        // Render each shape into the framebuffer (painter's algorithm: later shapes overwrite)
        for (auto& shape : layout_.shapes()) {
            auto it = widgetStates.find(shape->id);
            WidgetState state;
            if (it != widgetStates.end())
                state = it->second;

            auto style = visualStyleFromString(shape->visualStyle);

            if (style != VisualStyle::Static || state.active) {
                // Widget rendering: get per-pixel colored commands
                auto cmds = WidgetRenderer::renderWidget(*shape, state);
                for (auto& cmd : cmds) {
                    if (cmd.x >= 0 && cmd.x < W && cmd.y >= 0 && cmd.y < H) {
                        fb[cmd.y][cmd.x][0] = (uint8_t)cmd.color.r;
                        fb[cmd.y][cmd.x][1] = (uint8_t)cmd.color.g;
                        fb[cmd.y][cmd.x][2] = (uint8_t)cmd.color.b;
                    }
                }
            } else {
                // Static: all pixels get shape color
                auto pixels = shape->gridPixels();
                uint8_t r = (uint8_t)shape->color.r;
                uint8_t g = (uint8_t)shape->color.g;
                uint8_t b = (uint8_t)shape->color.b;
                for (auto& [px, py] : pixels) {
                    if (px >= 0 && px < W && py >= 0 && py < H) {
                        fb[py][px][0] = r;
                        fb[py][px][1] = g;
                        fb[py][px][2] = b;
                    }
                }
            }
        }

        // DAW feedback: brighten highlighted shape pixels on hardware
        if (processor_ && processor_->getDawFeedback().isEnabled()) {
            auto highlighted = processor_->getDawFeedback().getHighlightedShapes();
            for (auto& shape : layout_.shapes()) {
                if (highlighted.count(shape->id)) {
                    auto pixels = shape->gridPixels();
                    for (auto& [px, py] : pixels) {
                        if (px >= 0 && px < W && py >= 0 && py < H) {
                            fb[py][px][0] = (uint8_t)std::min(127, (int)fb[py][px][0] + 40);
                            fb[py][px][1] = (uint8_t)std::min(127, (int)fb[py][px][1] + 30);
                            fb[py][px][2] = (uint8_t)std::min(127, (int)fb[py][px][2] + 5);
                        }
                    }
                }
            }
        }

        // Per-finger colored dots on hardware surface
        if (processor_) {
            auto fingers = processor_->getActiveFingers();
            int fingerNum = 0;
            for (auto& [fid, fi] : fingers) {
                Color7 color = processor_->getPerFingerColors()
                    ? FingerPalette::colorForFinger(fingerNum)
                    : Color7{127, 127, 127};
                int gx = (int)std::round(fi.x);
                int gy = (int)std::round(fi.y);
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int px = gx + dx, py = gy + dy;
                        if (px >= 0 && px < W && py >= 0 && py < H) {
                            fb[py][px][0] = (uint8_t)color.r;
                            fb[py][px][1] = (uint8_t)color.g;
                            fb[py][px][2] = (uint8_t)color.b;
                        }
                    }
                }
                ++fingerNum;
            }
        }

        // Differential rendering: count changed pixels
        int changedCount = 0;
        if (!forceFullFrame_) {
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if (fb[y][x][0] != prevFb_[y][x][0] ||
                        fb[y][x][1] != prevFb_[y][x][1] ||
                        fb[y][x][2] != prevFb_[y][x][2])
                        ++changedCount;
        }

        // Use per-pixel updates when few pixels changed, full frame otherwise
        static constexpr int DIFF_THRESHOLD = 200;  // ~20% of 1008 pixels
        if (!forceFullFrame_ && changedCount > 0 && changedCount <= DIFF_THRESHOLD) {
            // Send only changed pixels via drawPixel (Y-flipped)
            for (int sy = 0; sy < H; ++sy) {
                int hy = (H - 1) - sy;
                for (int x = 0; x < W; ++x) {
                    if (fb[sy][x][0] != prevFb_[sy][x][0] ||
                        fb[sy][x][1] != prevFb_[sy][x][1] ||
                        fb[sy][x][2] != prevFb_[sy][x][2]) {
                        connection_.drawPixel(0, x, hy,
                            fb[sy][x][0], fb[sy][x][1], fb[sy][x][2]);
                    }
                }
            }
        } else if (forceFullFrame_ || changedCount > 0) {
            // Full frame: send drawImage (Y-flipped)
            std::vector<uint8_t> rgbData(W * H * 3);
            for (int sy = 0; sy < H; ++sy) {
                int hy = (H - 1) - sy;
                for (int x = 0; x < W; ++x) {
                    int idx = (hy * W + x) * 3;
                    rgbData[idx + 0] = fb[sy][x][0];
                    rgbData[idx + 1] = fb[sy][x][1];
                    rgbData[idx + 2] = fb[sy][x][2];
                }
            }
            connection_.drawImage(0, 0, 0, W, H, rgbData);
        }

        // Store current frame for next diff comparison
        std::memcpy(prevFb_, fb, sizeof(fb));
        forceFullFrame_ = false;
        lastWidgetStates_ = widgetStates;
        dirty_ = false;
    }

    // Keep timer running while connected — the timer callback is cheap
    // when nothing changes (just a map comparison), and we need it
    // running to detect new finger touches on visual widgets.
    if (!connection_.isConnected())
        stopTimer();
}

void EraeRenderer::render()
{
    // Rendering is now done entirely in timerCallback via framebuffer
}

void EraeRenderer::renderShape(const Shape& /*shape*/, const WidgetState& /*state*/)
{
    // No longer used — all rendering is done via framebuffer in timerCallback
}

} // namespace erae
