#include "core/LockPressCounter.hpp"

#include <cassert>

int main() {
    LockPressCounter counter;
    constexpr std::uint64_t kA = 1, kB = 2;

    counter.press(kA);
    counter.press(kA);
    assert(counter.count() == 2 && !counter.complete());
    counter.press(kA);
    assert(counter.complete());

    // A different input starts over at one, it does not clear to zero.
    counter.reset();
    counter.press(kA);
    counter.press(kA);
    counter.press(kB);
    assert(counter.count() == 1);
    counter.press(kA);
    assert(counter.count() == 1);

    // A pause inside the window keeps the count; a longer one drops it.
    counter.reset();
    counter.press(kA);
    counter.tick(LockPressCounter::kMaxSecondsBetweenPresses - 0.1f);
    counter.press(kA);
    assert(counter.count() == 2);
    counter.tick(LockPressCounter::kMaxSecondsBetweenPresses + 0.1f);
    assert(counter.count() == 0);
    counter.press(kA);
    assert(counter.count() == 1);
}
