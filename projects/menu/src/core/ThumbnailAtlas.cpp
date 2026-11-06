#include "ThumbnailAtlas.hpp"

#include "DebugLog.hpp"

#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Renderer.hpp>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace thumbs = switchu::thumbs;

namespace {

constexpr std::uint64_t kUiImageReserve = 8u * 1024u * 1024u;

bool headerMatches(const thumbs::FileHeader& header, const thumbs::AtlasLayout& layout) {
    const thumbs::FileHeader expected{};
    return header.magic == expected.magic && header.version == expected.version &&
           header.cellWidth == layout.cellWidth && header.cellHeight == layout.cellHeight &&
           header.columns == layout.columns && header.rows == layout.rows;
}

// Opens the file for in-place writes, creating it full-size when missing or
// from another layout.
FILE* openForPatch(const std::string& path, const thumbs::AtlasLayout& layout) {
    if (FILE* file = std::fopen(path.c_str(), "r+b")) {
        thumbs::FileHeader header{};
        // The size comes from this handle: on Horizon stat() opens the file
        // again, which fails while it is open for writing. That made every
        // valid atlas look broken, so each saved title recreated all of it and
        // only the last one survived: HOME baked the whole library again on
        // every start, 4 MB of SD writes per title.
        if (std::fread(&header, sizeof(header), 1, file) == 1 && headerMatches(header, layout) &&
            std::fseek(file, 0, SEEK_END) == 0 &&
            static_cast<std::size_t>(std::ftell(file)) == thumbs::fileBytes(layout))
            return file;
        std::fclose(file);
    }
    DebugLog::log("[thumbs] creating %s", path.c_str());
    mkdir("sdmc:/config", 0755);
    mkdir("sdmc:/config/SwitchU", 0755);
    mkdir("sdmc:/config/SwitchU/cache", 0755);
    FILE* file = std::fopen(path.c_str(), "w+b");
    if (!file) return nullptr;
    thumbs::FileHeader header{};
    header.cellWidth = layout.cellWidth;
    header.cellHeight = layout.cellHeight;
    header.columns = layout.columns;
    header.rows = layout.rows;
    const std::vector<std::uint8_t> zeros(64u * 1024u, 0);
    bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1;
    for (std::size_t left = thumbs::fileBytes(layout) - sizeof(header); ok && left > 0;) {
        const std::size_t chunk = std::min(left, zeros.size());
        ok = std::fwrite(zeros.data(), 1, chunk, file) == chunk;
        left -= chunk;
    }
    if (!ok) {
        std::fclose(file);
        std::remove(path.c_str());
        return nullptr;
    }
    return file;
}

void patchFile(const std::string& path, const thumbs::AtlasLayout& layout, std::mutex& mutex,
               int cell, std::uint64_t titleId, const std::vector<std::uint8_t>& pixels) {
    std::lock_guard lock(mutex);
    FILE* file = openForPatch(path, layout);
    if (!file) {
        DebugLog::log("[thumbs] could not open %s", path.c_str());
        return;
    }
    if (!pixels.empty()) {
        std::fseek(file, static_cast<long>(thumbs::cellOffset(layout, cell)), SEEK_SET);
        std::fwrite(pixels.data(), 1, pixels.size(), file);
    }
    // After the pixels: a crash in between leaves the old title on its old art.
    std::fseek(file, static_cast<long>(thumbs::titleOffset(cell)), SEEK_SET);
    std::fwrite(&titleId, sizeof(titleId), 1, file);
    std::fclose(file);
}

} // namespace

struct ThumbnailAtlas::LoadJob {
    std::vector<std::uint8_t> rgba;
    std::vector<std::uint64_t> titles;
    std::atomic<bool> done{false};
};

ThumbnailAtlas::ThumbnailAtlas(std::string path, thumbs::AtlasLayout layout)
    : m_path(std::move(path)), m_layout(layout), m_index(layout.capacity()) {}

void ThumbnailAtlas::startLoad(nxui::ThreadPool& pool) {
    if (m_loadStarted) return;
    m_loadStarted = true;
    auto job = std::make_shared<LoadJob>();
    m_loadJob = job;
    pool.submit([job, path = m_path, layout = m_layout, mutex = m_fileMutex]() {
        job->titles.assign(static_cast<std::size_t>(layout.capacity()), 0);
        job->rgba.assign(layout.atlasBytes(), 0);
        std::lock_guard lock(*mutex);
        FILE* file = std::fopen(path.c_str(), "rb");
        struct stat info{};
        thumbs::FileHeader header{};
        if (file && stat(path.c_str(), &info) == 0 &&
            static_cast<std::size_t>(info.st_size) == thumbs::fileBytes(layout) &&
            std::fread(&header, sizeof(header), 1, file) == 1 && headerMatches(header, layout) &&
            std::fread(job->titles.data(), sizeof(std::uint64_t), job->titles.size(), file) ==
                job->titles.size()) {
            // Cells are stored one after another; the texture wants them placed
            // on the grid, row by row of pixels.
            std::vector<std::uint8_t> cell(layout.cellBytes());
            const std::size_t rowBytes = static_cast<std::size_t>(layout.cellWidth) * 4u;
            const std::size_t atlasRow = static_cast<std::size_t>(layout.atlasWidth()) * 4u;
            for (int index = 0; index < layout.capacity(); ++index) {
                if (std::fread(cell.data(), 1, cell.size(), file) != cell.size()) {
                    job->titles[static_cast<std::size_t>(index)] = 0;
                    break;
                }
                const int column = index % layout.columns;
                const int row = index / layout.columns;
                for (int y = 0; y < layout.cellHeight; ++y) {
                    std::memcpy(job->rgba.data() +
                                    (static_cast<std::size_t>(row) * layout.cellHeight + y) * atlasRow +
                                    static_cast<std::size_t>(column) * rowBytes,
                                cell.data() + static_cast<std::size_t>(y) * rowBytes, rowBytes);
                }
            }
        } else {
            std::fill(job->titles.begin(), job->titles.end(), 0);
        }
        if (file) std::fclose(file);
        job->done.store(true);
    });
}

bool ThumbnailAtlas::poll(nxui::GpuDevice& gpu, nxui::Renderer& renderer) {
    if (!m_loadJob || !m_loadJob->done.load()) return false;
    const auto job = std::move(m_loadJob);
    m_loadJob.reset();
    // Never squeeze the UI for a nicety: without the atlas tiles just wait for
    // their real art, as they always did.
    if (gpu.imageMemoryAvailable() <= kUiImageReserve + job->rgba.size()) {
        DebugLog::log("[thumbs] skipped %s: low GPU memory", m_path.c_str());
        return false;
    }
    if (!m_texture.loadFromPixels(gpu, renderer, job->rgba.data(),
                                  m_layout.atlasWidth(), m_layout.atlasHeight())) {
        DebugLog::log("[thumbs] upload failed %s", m_path.c_str());
        return false;
    }
    m_index.assignAll(std::move(job->titles));
    DebugLog::log("[thumbs] loaded %s", m_path.c_str());
    return true;
}

void ThumbnailAtlas::unload() {
    m_loadJob.reset();
    m_texture = nxui::Texture{};
    m_loadStarted = false;
}

bool ThumbnailAtlas::sourceRect(std::uint64_t titleId, nxui::Rect& out) const {
    const int cell = ready() ? m_index.find(titleId) : -1;
    if (cell < 0) return false;
    out = {static_cast<float>(cell % m_layout.columns * m_layout.cellWidth),
           static_cast<float>(cell / m_layout.columns * m_layout.cellHeight),
           static_cast<float>(m_layout.cellWidth), static_cast<float>(m_layout.cellHeight)};
    return true;
}

bool ThumbnailAtlas::needsBake(std::uint64_t titleId) const {
    if (!ready()) return false;
    return m_index.find(titleId) < 0 ||
           std::find(m_stale.begin(), m_stale.end(), titleId) != m_stale.end();
}

void ThumbnailAtlas::markStale(std::uint64_t titleId) {
    if (std::find(m_stale.begin(), m_stale.end(), titleId) == m_stale.end())
        m_stale.push_back(titleId);
}

bool ThumbnailAtlas::put(std::uint64_t titleId, std::vector<std::uint8_t> pixels,
                         nxui::ThreadPool& pool) {
    if (!ready() || pixels.size() != m_layout.cellBytes()) return false;
    m_stale.erase(std::remove(m_stale.begin(), m_stale.end(), titleId), m_stale.end());
    const int cell = m_index.acquire(titleId);
    if (cell < 0) return false;
    if (!m_texture.updateRegion(pixels.data(), cell % m_layout.columns * m_layout.cellWidth,
                                cell / m_layout.columns * m_layout.cellHeight,
                                m_layout.cellWidth, m_layout.cellHeight))
        return false;
    pool.submit([path = m_path, layout = m_layout, mutex = m_fileMutex, cell, titleId,
                 pixels = std::move(pixels)]() {
        patchFile(path, layout, *mutex, cell, titleId, pixels);
    });
    return true;
}

void ThumbnailAtlas::erase(std::uint64_t titleId, nxui::ThreadPool& pool) {
    m_stale.erase(std::remove(m_stale.begin(), m_stale.end(), titleId), m_stale.end());
    const int cell = m_index.release(titleId);
    if (cell < 0) return;
    pool.submit([path = m_path, layout = m_layout, mutex = m_fileMutex, cell]() {
        patchFile(path, layout, *mutex, cell, 0, {});
    });
}
