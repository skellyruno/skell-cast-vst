#pragma once
#include <cstddef>

namespace skellcast {
struct OutputCandidate {
    bool isWindowsDefault;
    bool hasActiveSession;
    bool isVirtual;
    bool sessionInspectionReliable;
};

// Safest available spare playback device. NEVER fall back to the Windows defaults
// (where the host is likely already rendering). Prefer virtual devices for OBS.
constexpr int selectSpareOutput(const OutputCandidate* candidates, std::size_t size) noexcept {
    int selected = -1;
    for (std::size_t i = 0; i < size; ++i) {
        const auto& c = candidates[i];
        if (c.isWindowsDefault || c.hasActiveSession || !c.sessionInspectionReliable)
            continue;
        if (selected < 0 || (c.isVirtual && !candidates[selected].isVirtual))
            selected = static_cast<int>(i);
    }
    return selected;
}
} // namespace skellcast
