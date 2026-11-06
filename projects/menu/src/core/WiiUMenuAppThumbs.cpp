// Thumbnail atlases: every title's art, small, available the moment HOME opens.
// Missing entries are baked one at a time on a worker while the UI is idle.
#include "WiiUMenuApp.hpp"
#include "widgets/GlossyIcon.hpp"
#include "DebugLog.hpp"
#include "launcher/AppListLoader.hpp"
#include "launcher/IconStreamer.hpp"
#include "steamgriddb/ArtworkCache.hpp"
#include "steamgriddb/SteamGridDbManager.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>

namespace thumbs = switchu::thumbs;

struct WiiUMenuApp::ThumbFillJob {
    std::uint64_t titleId = 0;
    bool wantIcon = false;
    bool wantWide = false;
    std::vector<std::uint8_t> icon;
    std::vector<std::uint8_t> wide;
    std::atomic<bool> done{false};
};

namespace {

constexpr float kThumbFillIdleSeconds = 0.4f;
constexpr float kWideCheckSeconds = 1.f;

std::vector<std::uint8_t> bakeIcon(std::uint64_t titleId, const thumbs::AtlasLayout& layout) {
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
    if (SteamGridDbManager::hasIcon(titleId)) {
        // Also leaves the baked full-size cache the streamer reads later.
        auto decoded = steamgriddb::artwork::decodeFit(
            SteamGridDbManager::iconPath(titleId), 320, 480);
        rgba = std::move(decoded.rgba);
        width = decoded.width;
        height = decoded.height;
    }
    if (rgba.empty()) {
        auto decoded = IconStreamer::decodeIconData(AppListLoader::loadIconData(titleId, false));
        rgba = std::move(decoded.rgba);
        width = decoded.w;
        height = decoded.h;
    }
    if (rgba.empty()) return {};
    std::vector<std::uint8_t> cell(layout.cellBytes());
    thumbs::scaleCover(rgba.data(), width, height, cell.data(),
                       layout.cellWidth, layout.cellHeight);
    return cell;
}

// Same priority as the live wide tile: landscape grid art is a complete face,
// otherwise hero with the logo on top. Empty when the title has neither.
std::vector<std::uint8_t> bakeWideCard(std::uint64_t titleId, const thumbs::AtlasLayout& layout) {
    namespace artwork = steamgriddb::artwork;
    std::error_code error;
    const std::string gridWide = SteamGridDbManager::gridWidePath(titleId);
    std::vector<std::uint8_t> cell(layout.cellBytes());
    if (std::filesystem::is_regular_file(gridWide, error)) {
        const auto face = artwork::decode(gridWide, 460, 215, true);
        if (!face.rgba.empty()) {
            thumbs::scaleCover(face.rgba.data(), face.width, face.height, cell.data(),
                               layout.cellWidth, layout.cellHeight);
            return cell;
        }
    }
    error.clear();
    const std::string heroPath = SteamGridDbManager::heroPath(titleId);
    if (!std::filesystem::is_regular_file(heroPath, error)) return {};
    const auto hero = artwork::decode(heroPath, 640, 360, true);
    if (hero.rgba.empty()) return {};
    thumbs::scaleCover(hero.rgba.data(), hero.width, hero.height, cell.data(),
                       layout.cellWidth, layout.cellHeight);
    error.clear();
    const std::string logoPath = SteamGridDbManager::logoPath(titleId);
    if (std::filesystem::is_regular_file(logoPath, error)) {
        const auto logo = artwork::decode(logoPath, 384, 192, false);
        // Fractions of GlossyIcon's logoArea on the wide card.
        const int boxX = layout.cellWidth * 4 / 100;
        const int boxY = layout.cellHeight * 58 / 100;
        const int boxWidth = layout.cellWidth * 58 / 100;
        const int boxHeight = layout.cellHeight * 32 / 100;
        if (!logo.rgba.empty())
            thumbs::blendLogo(logo.rgba.data(), logo.width, logo.height, cell.data(),
                              layout.cellWidth, layout.cellHeight,
                              boxX, boxY, boxWidth, boxHeight);
    }
    return cell;
}

} // namespace

void WiiUMenuApp::applyThumbnailsToIcon(GlossyIcon& icon) const {
    if (icon.entryKind() != GridEntryKind::Application) return;
    nxui::Rect iconSource{};
    nxui::Rect wideSource{};
    const bool hasIcon = m_iconThumbs.sourceRect(icon.titleId(), iconSource);
    const bool hasWide = m_wideThumbs.sourceRect(icon.titleId(), wideSource);
    icon.setThumbnails(hasIcon ? m_iconThumbs.texture() : nullptr, iconSource,
                       hasWide ? m_wideThumbs.texture() : nullptr, wideSource);
}

void WiiUMenuApp::syncThumbnailsToIcons(std::uint64_t onlyTitleId) {
    if (!m_grid) return;
    for (const auto& icon : m_grid->allIcons()) {
        if (icon && (onlyTitleId == 0 || icon->titleId() == onlyTitleId))
            applyThumbnailsToIcon(*icon);
    }
}

void WiiUMenuApp::markThumbnailsStale(std::uint64_t titleId) {
    m_iconThumbs.markStale(titleId);
    m_wideThumbs.markStale(titleId);
    m_thumbFillSkipped.erase(titleId);
    m_thumbFillCursor = 0;
}

void WiiUMenuApp::releaseThumbnailAtlases(const char* reason) {
    if (!m_iconThumbs.loadStarted() && !m_wideThumbs.loadStarted()) return;
    // The icons must stop pointing at the textures before they go away.
    if (m_grid) {
        for (const auto& icon : m_grid->allIcons()) {
            if (icon) icon->setThumbnails(nullptr, {}, nullptr, {});
        }
    }
    m_iconThumbs.unload();
    m_wideThumbs.unload();
    DebugLog::log("[thumbs] released (%s)", reason);
}

void WiiUMenuApp::pollThumbnailAtlases(float dt) {
    // Frame Tap counts every MiB; thumbnails come back when it is done.
    if (m_frameTapLightActive || m_iconStreamer.pageCacheRadiusCap() == 0 || !m_grid) return;

    m_iconThumbs.startLoad(m_threadPool);
    // Most libraries have no wide tiles: only then pay for this atlas.
    m_thumbWideCheckIn -= dt;
    if (!m_wideThumbs.loadStarted() && m_thumbWideCheckIn <= 0.f) {
        m_thumbWideCheckIn = kWideCheckSeconds;
        const bool anyWide = std::any_of(m_allApps.begin(), m_allApps.end(),
            [this](const AppEntry& entry) {
                return entry.isApplication() && switchu::widgets::isLandscapeWideGameTile(
                    gameGridSize(entry.titleId, AppLayoutMode::Grid));
            });
        if (anyWide) m_wideThumbs.startLoad(m_threadPool);
    }

    // One large upload per frame, and never in HOME's first deferred frames.
    if (m_deferredInitialAssetFrames == 0) {
        if (m_iconThumbs.poll(app().gpu(), app().renderer()) ||
            m_wideThumbs.poll(app().gpu(), app().renderer()))
            syncThumbnailsToIcons(0);
    }

    if (m_thumbFillJob && m_thumbFillJob->done.load()) {
        const auto job = std::move(m_thumbFillJob);
        m_thumbFillJob.reset();
        bool changed = false;
        if (job->wantIcon) {
            if (job->icon.empty())
                m_thumbFillSkipped.insert(job->titleId);
            else
                changed = m_iconThumbs.put(job->titleId, std::move(job->icon), m_threadPool);
        }
        if (job->wantWide) {
            if (job->wide.empty()) {
                // The art was removed, or there never was any.
                m_wideThumbs.erase(job->titleId, m_threadPool);
                m_thumbFillSkipped.insert(job->titleId);
                changed = true;
            } else {
                changed = m_wideThumbs.put(job->titleId, std::move(job->wide), m_threadPool)
                    || changed;
            }
        }
        if (changed) syncThumbnailsToIcons(job->titleId);
    }

    m_thumbIdleSeconds = app().input().hasUserActivity() ? 0.f : m_thumbIdleSeconds + dt;
    if (m_thumbFillJob || m_thumbIdleSeconds < kThumbFillIdleSeconds ||
        m_grid->isTransitioning() || leaveSplashActive() ||
        m_deferredInitialAssetFrames > 0 || !m_iconThumbs.ready())
        return;
    // Thumbnails are a nicety: what is on screen gets the SD and the workers first.
    if (m_appLayoutMode != AppLayoutMode::DynamicLine &&
        m_iconStreamer.needsVisibleLoads(m_grid->currentPage(), m_grid->iconsPerPage()))
        return;

    // Look at a bounded number of titles per frame, however large the library.
    constexpr std::size_t kTitlesScannedPerFrame = 16;
    for (std::size_t scanned = 0;
         scanned < kTitlesScannedPerFrame && m_thumbFillCursor < m_allApps.size();
         ++scanned, ++m_thumbFillCursor) {
        const AppEntry& entry = m_allApps[m_thumbFillCursor];
        if (!entry.isApplication() || entry.titleId == 0 ||
            m_thumbFillSkipped.count(entry.titleId))
            continue;
        const bool wantIcon = m_iconThumbs.needsBake(entry.titleId);
        const bool wantWide = m_wideThumbs.needsBake(entry.titleId) &&
            switchu::widgets::isLandscapeWideGameTile(
                gameGridSize(entry.titleId, AppLayoutMode::Grid));
        if (!wantIcon && !wantWide)
            continue;
        auto job = std::make_shared<ThumbFillJob>();
        job->titleId = entry.titleId;
        job->wantIcon = wantIcon;
        job->wantWide = wantWide;
        m_thumbFillJob = job;
        m_threadPool.submit([job, iconLayout = m_iconThumbs.layout(),
                             wideLayout = m_wideThumbs.layout()]() {
            if (job->wantIcon) job->icon = bakeIcon(job->titleId, iconLayout);
            if (job->wantWide) job->wide = bakeWideCard(job->titleId, wideLayout);
            job->done.store(true);
        });
        break;
    }
}
