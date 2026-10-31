#include "core/SwitifyProtocol.hpp"

#include <cassert>
#include <cstring>
#include <string>

namespace sw = switchu::switify;

int main() {
    // FNV-1a 64 reference vectors: Switify names cached covers with it.
    assert(sw::fnv1a64("") == 0xcbf29ce484222325ULL);
    assert(sw::fnv1a64("a") == 0xaf63dc4c8601ec8cULL);
    assert(sw::hashName("a") == "af63dc4c8601ec8c");
    assert(sw::switifyCoverPath("a") ==
           "sdmc:/config/Switify/artwork-cache/af63dc4c8601ec8c.img");
    assert(sw::ownCoverPath("a") ==
           "sdmc:/config/SwitchU/cache/switify/af63dc4c8601ec8c.jpg");

    // SwitchU repeat (Off, All, One) <-> Switify repeat (Off, One, All).
    auto flagsFor = [](std::uint32_t switifyMode) {
        return switifyMode == 1 ? sw::kStatusFlagRepeatOne
             : switifyMode == 2 ? sw::kStatusFlagRepeatAll : 0u;
    };
    static_assert(sw::switifyRepeatFromMenu(0) == 0);
    static_assert(sw::switifyRepeatFromMenu(1) == 2);
    static_assert(sw::switifyRepeatFromMenu(2) == 1);
    for (int menu = 0; menu < 3; ++menu)
        assert(sw::menuRepeatFromFlags(flagsFor(sw::switifyRepeatFromMenu(menu))) == menu);
    // The overlay cycles Off -> All -> One like SwitchU's L3 does.
    assert(sw::menuRepeatFromFlags(sw::kStatusFlagRepeatAll | sw::kStatusFlagShuffle) == 1);

    // Fixed-size wire strings are not always NUL-terminated.
    sw::IpcStatus status{};
    std::memset(status.title, 'x', sizeof(status.title));
    assert(sw::boundedString(status.title).size() == sizeof(status.title));
    std::strcpy(status.artist, "Artist");
    assert(sw::boundedString(status.artist) == "Artist");
}
