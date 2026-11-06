#pragma once

#include "ThumbnailIndex.hpp"

#include <nxui/core/Texture.hpp>
#include <nxui/core/ThreadPool.hpp>
#include <nxui/core/Types.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Small copies of the game art, in one GPU texture and one file on the SD. It
// survives HOME's cold relaunch: a single read brings back every title at once,
// so a tile never sits empty while its full-size art streams in.
class ThumbnailAtlas {
public:
    ThumbnailAtlas(std::string path, switchu::thumbs::AtlasLayout layout);

    const switchu::thumbs::AtlasLayout& layout() const { return m_layout; }

    // Reads the file on a worker; poll() uploads the result.
    void startLoad(nxui::ThreadPool& pool);
    bool loadStarted() const { return m_loadStarted; }
    // True on the frame the texture became available.
    bool poll(nxui::GpuDevice& gpu, nxui::Renderer& renderer);
    // Gives the GPU memory back (Frame Tap headroom). Waits for the GPU.
    void unload();

    bool ready() const { return m_texture.valid(); }
    const nxui::Texture* texture() const { return &m_texture; }

    bool sourceRect(std::uint64_t titleId, nxui::Rect& out) const;
    bool needsBake(std::uint64_t titleId) const;
    void markStale(std::uint64_t titleId);

    // pixels: exactly one cell. Updates the texture in place and the file on a
    // worker. False when the atlas is full or not loaded.
    bool put(std::uint64_t titleId, std::vector<std::uint8_t> pixels, nxui::ThreadPool& pool);
    void erase(std::uint64_t titleId, nxui::ThreadPool& pool);

private:
    struct LoadJob;

    std::string m_path;
    switchu::thumbs::AtlasLayout m_layout;
    switchu::thumbs::Index m_index;
    std::vector<std::uint64_t> m_stale;
    nxui::Texture m_texture;
    std::shared_ptr<LoadJob> m_loadJob;
    bool m_loadStarted = false;
    // Serializes writes to the file between pool workers.
    std::shared_ptr<std::mutex> m_fileMutex = std::make_shared<std::mutex>();
};
