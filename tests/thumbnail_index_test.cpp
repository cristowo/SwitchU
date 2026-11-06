#include "core/ThumbnailIndex.hpp"

#include <cassert>
#include <vector>

namespace thumbs = switchu::thumbs;

int main() {
    // File layout: cells follow the header and the title table, without overlap.
    const thumbs::AtlasLayout layout{64, 64, 16, 16};
    assert(layout.capacity() == 256);
    assert(layout.atlasBytes() == 4u * 1024u * 1024u);
    assert(thumbs::titleOffset(0) == sizeof(thumbs::FileHeader));
    assert(thumbs::cellOffset(layout, 0) == thumbs::titleOffset(256));
    assert(thumbs::cellOffset(layout, 1) - thumbs::cellOffset(layout, 0) == layout.cellBytes());
    assert(thumbs::fileBytes(layout) == thumbs::cellOffset(layout, 0) + layout.atlasBytes());

    // Index: stable cells, reuse after release, a clean refusal when full.
    thumbs::Index index(2);
    assert(index.find(7) == -1);
    assert(index.acquire(0) == -1);
    assert(index.acquire(7) == 0);
    assert(index.acquire(7) == 0);
    assert(index.acquire(8) == 1);
    assert(index.acquire(9) == -1);
    assert(index.release(7) == 0);
    assert(index.find(7) == -1);
    assert(index.acquire(9) == 0);
    assert(index.release(123) == -1);

    // Cover scale: a flat colour stays flat, and a wide source crops its sides.
    std::vector<std::uint8_t> source(16u * 8u * 4u, 0);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 16; ++x) {
            std::uint8_t* pixel = source.data() + (y * 16 + x) * 4;
            // Outer quarter columns are red, the middle half is green.
            const bool middle = x >= 4 && x < 12;
            pixel[0] = middle ? 0 : 255;
            pixel[1] = middle ? 255 : 0;
            pixel[3] = 255;
        }
    }
    std::vector<std::uint8_t> cell(4u * 4u * 4u, 0);
    thumbs::scaleCover(source.data(), 16, 8, cell.data(), 4, 4);
    for (int i = 0; i < 16; ++i) {
        assert(cell[static_cast<std::size_t>(i) * 4u] == 0);
        assert(cell[static_cast<std::size_t>(i) * 4u + 1u] == 255);
        assert(cell[static_cast<std::size_t>(i) * 4u + 3u] == 255);
    }

    // Logo blend: transparent pixels keep the card, opaque ones replace it.
    const std::uint8_t logo[8] = {10, 20, 30, 255, 200, 200, 200, 0};
    std::vector<std::uint8_t> card(2u * 1u * 4u, 100);
    thumbs::blendLogo(logo, 2, 1, card.data(), 2, 1, 0, 0, 2, 1);
    assert(card[0] == 10 && card[1] == 20 && card[2] == 30);
    assert(card[4] == 100 && card[5] == 100 && card[6] == 100);
}
