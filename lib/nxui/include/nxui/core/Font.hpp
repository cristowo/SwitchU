#pragma once
#include <nxui/core/Types.hpp>
#include "Texture.hpp"
#include <SDL2/SDL_ttf.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <list>

namespace nxui {

class Renderer;
class GpuDevice;

class Font {
public:
    Font() = default;
    ~Font();

    bool load(GpuDevice& gpu, Renderer& ren,
              const std::string& path, int ptSize);
    // Take a font opened elsewhere (on a worker, its glyphs already drawn once:
    // the first drawing of each is the slow part). Owned from here on.
    bool adopt(GpuDevice& gpu, Renderer& ren, TTF_Font* opened, int ptSize);

    // Render a string to a texture (cached) and draw it
    void draw(Renderer& ren, const std::string& text,
              const Vec2& pos, const Color& color, float scale = 1.f);

    // Measure text dimensions
    Vec2 measure(const std::string& text) const;

    // Clear all cached glyph textures (call before GPU descriptor reset)
    void clearCache();

    bool maintenanceRequested() const { return m_maintenanceRequested; }
    std::size_t cacheBytes() const { return m_cacheBytes; }
    std::size_t cacheEntryCount() const { return m_lruList.size(); }
    void trimCache(std::size_t maxEntries = 72,
                   std::size_t maxBytes = 2u * 1024u * 1024u);

    int ptSize() const { return m_ptSize; }
    std::uint64_t revision() const { return m_revision; }

private:
    // Render full string to texture (cache by string)
    Texture* getOrRender(GpuDevice& gpu, Renderer& ren, const std::string& text);
public:
    // New strings rendered so far, and the time they took (startup profiling).
    static inline unsigned s_renderCount = 0;
    static inline std::uint64_t s_renderMicros = 0;
    static inline std::uint64_t s_ttfMicros = 0; // the SDL_ttf part of it
    // New strings rendered while this is on are kept (to learn which letters
    // a start needs).
    void setRecordRendered(bool on) { m_record = on; }
    std::string takeRecorded() { m_record = false; return std::move(m_recorded); }
    // Draw one character once: it is cached before a label needs it.
    void warmGlyph(std::uint32_t codepoint);
private:
    bool m_record = false;
    std::string m_recorded;

    TTF_Font* m_font = nullptr;
    int       m_ptSize = 0;
    GpuDevice* m_gpu = nullptr;
    Renderer*  m_ren = nullptr;
    std::uint64_t m_revision = 0;

    static constexpr std::size_t kMaxCacheEntries = 144;
    static constexpr std::size_t kMaxCacheBytes = 4u * 1024u * 1024u;
    static constexpr std::size_t kGpuHeadroom = 768u * 1024u;

    struct CacheEntry {
        std::string key;
        Texture     tex;
        int         w = 0, h = 0;
    };
    using LruList = std::list<CacheEntry>;
    using LruMap  = std::unordered_map<std::string, LruList::iterator>;

    LruList m_lruList;
    LruMap  m_lruMap;
    std::size_t m_cacheBytes = 0;
    bool m_maintenanceRequested = false;
};

} // namespace nxui
