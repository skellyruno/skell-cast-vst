#pragma once

namespace skellcast {
// A stable 0/1 state saved with the FL Studio project.
enum class OutputTarget { Cable, Desktop };

constexpr OutputTarget outputTargetFromParameter(float value) noexcept {
    return value >= 0.5f ? OutputTarget::Desktop : OutputTarget::Cable;
}
} // namespace skellcast
