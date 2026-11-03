#include <phantasm/geometry.hpp>

namespace phantasm {

bool intersects(const Rect& a, const Rect& b) noexcept {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

} // namespace phantasm
