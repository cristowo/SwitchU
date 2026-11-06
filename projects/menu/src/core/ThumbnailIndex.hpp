#pragma once

// Pure parts of the thumbnail atlas: cell bookkeeping, file layout and pixel
// scaling. No libnx or GPU here so the host test can check them.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace switchu::thumbs {

struct AtlasLayout {
    int cellWidth = 0;
    int cellHeight = 0;
    int columns = 0;
    int rows = 0;

    int capacity() const { return columns * rows; }
    int atlasWidth() const { return cellWidth * columns; }
    int atlasHeight() const { return cellHeight * rows; }
    std::size_t cellBytes() const {
        return static_cast<std::size_t>(cellWidth) * cellHeight * 4u;
    }
    std::size_t atlasBytes() const { return cellBytes() * capacity(); }
};

// On disk: FileHeader, one titleId per cell, then the cells one after another,
// so filling a cell is a small in-place write instead of rewriting megabytes.
struct FileHeader {
    std::uint32_t magic = 0x41545553; // "SUTA"
    std::uint32_t version = 1;
    std::int32_t cellWidth = 0;
    std::int32_t cellHeight = 0;
    std::int32_t columns = 0;
    std::int32_t rows = 0;
};

inline std::size_t titleOffset(int cell) {
    return sizeof(FileHeader) + static_cast<std::size_t>(cell) * sizeof(std::uint64_t);
}

inline std::size_t cellOffset(const AtlasLayout& layout, int cell) {
    return titleOffset(layout.capacity()) + layout.cellBytes() * static_cast<std::size_t>(cell);
}

inline std::size_t fileBytes(const AtlasLayout& layout) {
    return cellOffset(layout, layout.capacity());
}

// Linear scan: at most a few hundred cells, looked up on rare events only.
// ponytail: full atlas = later titles show no thumbnail; add a second page or
// LRU eviction if libraries beyond capacity become common.
class Index {
public:
    explicit Index(int capacity = 0) : m_titles(static_cast<std::size_t>(capacity), 0) {}

    const std::vector<std::uint64_t>& titles() const { return m_titles; }
    void assignAll(std::vector<std::uint64_t> titles) { m_titles = std::move(titles); }

    int find(std::uint64_t titleId) const {
        if (titleId == 0) return -1;
        const auto it = std::find(m_titles.begin(), m_titles.end(), titleId);
        return it == m_titles.end() ? -1 : static_cast<int>(it - m_titles.begin());
    }

    // The title's cell, reusing a free one if it has none. -1 when full.
    int acquire(std::uint64_t titleId) {
        if (titleId == 0) return -1;
        if (const int cell = find(titleId); cell >= 0) return cell;
        const auto it = std::find(m_titles.begin(), m_titles.end(), std::uint64_t{0});
        if (it == m_titles.end()) return -1;
        *it = titleId;
        return static_cast<int>(it - m_titles.begin());
    }

    int release(std::uint64_t titleId) {
        const int cell = find(titleId);
        if (cell >= 0) m_titles[static_cast<std::size_t>(cell)] = 0;
        return cell;
    }

private:
    std::vector<std::uint64_t> m_titles;
};

// Box-filters src (RGBA) into dst, cropping like the tiles' cover fit.
inline void scaleCover(const std::uint8_t* src, int srcWidth, int srcHeight,
                       std::uint8_t* dst, int dstWidth, int dstHeight) {
    if (!src || !dst || srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0)
        return;
    int cropWidth = srcWidth;
    int cropHeight = srcHeight;
    if (static_cast<long long>(srcWidth) * dstHeight > static_cast<long long>(srcHeight) * dstWidth)
        cropWidth = std::max(1, static_cast<int>(static_cast<long long>(srcHeight) * dstWidth / dstHeight));
    else
        cropHeight = std::max(1, static_cast<int>(static_cast<long long>(srcWidth) * dstHeight / dstWidth));
    const int offsetX = (srcWidth - cropWidth) / 2;
    const int offsetY = (srcHeight - cropHeight) / 2;

    for (int y = 0; y < dstHeight; ++y) {
        const int y0 = offsetY + y * cropHeight / dstHeight;
        const int y1 = std::max(y0 + 1, offsetY + (y + 1) * cropHeight / dstHeight);
        for (int x = 0; x < dstWidth; ++x) {
            const int x0 = offsetX + x * cropWidth / dstWidth;
            const int x1 = std::max(x0 + 1, offsetX + (x + 1) * cropWidth / dstWidth);
            unsigned sum[4] = {0, 0, 0, 0};
            for (int sy = y0; sy < y1; ++sy) {
                const std::uint8_t* pixel =
                    src + (static_cast<std::size_t>(sy) * srcWidth + x0) * 4u;
                for (int sx = x0; sx < x1; ++sx, pixel += 4) {
                    for (int c = 0; c < 4; ++c) sum[c] += pixel[c];
                }
            }
            const unsigned count = static_cast<unsigned>((y1 - y0) * (x1 - x0));
            std::uint8_t* out = dst + (static_cast<std::size_t>(y) * dstWidth + x) * 4u;
            for (int c = 0; c < 4; ++c)
                out[c] = static_cast<std::uint8_t>((sum[c] + count / 2) / count);
        }
    }
}

// Alpha-blends a logo into dst inside the box given in dst pixels, fitted and
// bottom-left aligned the way the wide tile places its live logo.
inline void blendLogo(const std::uint8_t* logo, int logoWidth, int logoHeight,
                      std::uint8_t* dst, int dstWidth, int dstHeight,
                      int boxX, int boxY, int boxWidth, int boxHeight) {
    if (!logo || logoWidth <= 0 || logoHeight <= 0 || boxWidth <= 0 || boxHeight <= 0)
        return;
    int width = boxWidth;
    int height = static_cast<int>(static_cast<long long>(boxWidth) * logoHeight / logoWidth);
    if (height > boxHeight) {
        height = boxHeight;
        width = static_cast<int>(static_cast<long long>(boxHeight) * logoWidth / logoHeight);
    }
    width = std::max(1, width);
    height = std::max(1, height);
    const int startY = boxY + (boxHeight - height) / 2;
    for (int y = 0; y < height; ++y) {
        const int targetY = startY + y;
        if (targetY < 0 || targetY >= dstHeight) continue;
        const int sourceY = y * logoHeight / height;
        for (int x = 0; x < width; ++x) {
            const int targetX = boxX + x;
            if (targetX < 0 || targetX >= dstWidth) continue;
            const int sourceX = x * logoWidth / width;
            const std::uint8_t* in =
                logo + (static_cast<std::size_t>(sourceY) * logoWidth + sourceX) * 4u;
            std::uint8_t* out =
                dst + (static_cast<std::size_t>(targetY) * dstWidth + targetX) * 4u;
            const unsigned alpha = in[3];
            for (int c = 0; c < 3; ++c)
                out[c] = static_cast<std::uint8_t>((in[c] * alpha + out[c] * (255u - alpha) + 127u) / 255u);
            out[3] = std::max(out[3], in[3]);
        }
    }
}

} // namespace switchu::thumbs
