#include "core/LockPasswordEntry.hpp"

#include <cassert>

using Entry = LockPasswordEntry;

int main() {
    Entry e;

    // Unlock: the check only runs once the right number of presses is in.
    int checks = 0;
    e.startUnlock(4, [&checks](const std::string& p) { ++checks; return p == "UDLR"; });
    assert(e.press('U') == Entry::Result::None);
    assert(e.press('D') == Entry::Result::None);
    assert(e.press('L') == Entry::Result::None);
    assert(checks == 0);
    assert(e.press('R') == Entry::Result::Unlocked);
    assert(checks == 1);

    // A wrong try clears, shakes and names itself, and the next try starts fresh.
    for (char c : {'U', 'U', 'U'})
        e.press(c);
    assert(e.press('U') == Entry::Result::Wrong);
    assert(e.presses().empty());
    assert(e.failure() == Entry::Result::Wrong);
    e.update(1.f);
    assert(e.failure() == Entry::Result::None);
    for (char c : {'U', 'D', 'L'})
        e.press(c);
    assert(e.press('R') == Entry::Result::Unlocked);

    // Making it: too short is refused and keeps the presses.
    e.startMaking();
    for (char c : {'U', 'D', 'L'})
        e.press(c);
    assert(e.confirm() == Entry::Result::TooShort);
    assert(e.mode() == Entry::Mode::Make);
    assert(e.presses().size() == 3);

    // Same again finishes it.
    e.press('R');
    assert(e.confirm() == Entry::Result::None);
    assert(e.mode() == Entry::Mode::Again);
    assert(e.dots() == 4);
    for (char c : {'U', 'D', 'L'})
        assert(e.press(c) == Entry::Result::None);
    assert(e.press('R') == Entry::Result::Made);
    assert(e.made() == "UDLR");

    // Different again goes back to the start.
    e.startMaking();
    for (char c : {'U', 'U', 'U', 'U'})
        e.press(c);
    e.confirm();
    for (char c : {'U', 'U', 'U'})
        e.press(c);
    assert(e.press('D') == Entry::Result::Mismatch);
    assert(e.mode() == Entry::Mode::Make);
    assert(e.failure() == Entry::Result::Mismatch);

    // Never more than the longest password.
    e.startMaking();
    for (int i = 0; i < 12; ++i)
        e.press('L');
    assert(e.presses().size() == Entry::kMax);
    return 0;
}
