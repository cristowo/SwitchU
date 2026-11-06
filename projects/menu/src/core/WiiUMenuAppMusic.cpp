// Music sources: songs on the SD card (AudioManager) or Spotify through the
// Switify background service. Quick Settings (L), the "Now playing" widget
// and HOME's own BGM all go through these functions so that exactly one
// source plays at a time.
#include "WiiUMenuApp.hpp"
#include "widgets/GlossyIcon.hpp"
#include "DebugLog.hpp"
#ifdef SWITCHU_MENU
#include "smi_commands.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <functional>
#include <nxui/core/I18n.hpp>
#include <nxui/third_party/stb/stb_image.h>

namespace sw = switchu::switify;

namespace {

constexpr float kMusicLinkPeriod = 0.25f;
constexpr auto kPendingVolumeHold = std::chrono::milliseconds(1500);

enum SpotifyRootRow { RootQueue, RootPlaylist, RootLibrary, RootHome, RootSearch, RootCount };

std::string switifyStatusText(const sw::NowPlaying& now) {
    auto& i18n = nxui::I18n::instance();
    switch (now.state) {
        case sw::ServiceState::NotInstalled:
            return i18n.tr("switify.not_installed", "Switify is not installed");
        case sw::ServiceState::NotRunning:
            return i18n.tr("switify.not_running", "Switify is not running");
        case sw::ServiceState::Starting:
            return i18n.tr("switify.starting", "Starting Switify...");
        case sw::ServiceState::WaitingForCredentials:
            return i18n.tr("switify.sign_in", "Sign in with the Switify app");
        case sw::ServiceState::Connecting:
            return i18n.tr("switify.connecting", "Connecting...");
        case sw::ServiceState::Ready:
            break;
    }
    return now.hasTrack ? std::string() : i18n.tr("switify.nothing", "Nothing playing");
}

std::string clockText(float seconds) {
    const int total = std::max(0, static_cast<int>(seconds));
    const int rest = total % 60;
    return std::to_string(total / 60) + (rest < 10 ? ":0" : ":") + std::to_string(rest);
}

// "1:23 / 4:50" and how far along, for the 2x2 widget.
// The two main colours of a cover: its pixels sampled into coarse bins, the
// most weighty bin first (colourful ones weigh more than greys), then the
// weightiest one clearly unlike it. One pass over a few hundred pixels.
void coverColours(const std::uint8_t* rgba, std::size_t pixels, nxui::Color& first,
                  nxui::Color& second) {
    struct Bin { float r = 0, g = 0, b = 0, weight = 0; int count = 0; };
    Bin bins[64];
    const std::size_t step = std::max<std::size_t>(1, pixels / 256);
    for (std::size_t i = 0; i < pixels; i += step) {
        const float r = rgba[i * 4] / 255.f, g = rgba[i * 4 + 1] / 255.f, b = rgba[i * 4 + 2] / 255.f;
        const float hi = std::max({r, g, b}), lo = std::min({r, g, b});
        float weight = 0.3f + (hi > 0.f ? (hi - lo) / hi : 0.f);
        if (hi < 0.08f || lo > 0.92f) weight *= 0.3f; // near black or white
        auto& bin = bins[(static_cast<int>(r * 3.99f) << 4) | (static_cast<int>(g * 3.99f) << 2) |
                         static_cast<int>(b * 3.99f)];
        bin.r += r; bin.g += g; bin.b += b; bin.weight += weight; ++bin.count;
    }
    const auto colour = [](const Bin& bin) {
        return nxui::Color(bin.r / bin.count, bin.g / bin.count, bin.b / bin.count, 1.f);
    };
    int best = -1, next = -1;
    for (int i = 0; i < 64; ++i)
        if (bins[i].count && (best < 0 || bins[i].weight > bins[best].weight)) best = i;
    if (best < 0) return;
    first = colour(bins[best]);
    for (int i = 0; i < 64; ++i) {
        if (!bins[i].count || i == best) continue;
        const nxui::Color c = colour(bins[i]);
        const float apart = std::fabs(c.r - first.r) + std::fabs(c.g - first.g) + std::fabs(c.b - first.b);
        if (apart > 0.35f && (next < 0 || bins[i].weight > bins[next].weight)) next = i;
    }
    second = next >= 0 ? colour(bins[next])
                       : nxui::Color(first.r * 0.5f, first.g * 0.5f, first.b * 0.5f, 1.f);
}

template <class View>
void setProgress(View& view, float position, float duration) {
    if (duration <= 0.f) return;
    view.progress = std::clamp(position / duration, 0.f, 1.f);
    view.time = clockText(position) + " / " + clockText(duration);
}

} // namespace

void WiiUMenuApp::pauseLocalForSpotify() {
    if (!m_audio.isPlaying()) return;
    m_audio.pause();
    m_config.musicEnabled = false;
    persistMusicPlaybackState(false);
    m_config.save();
}

void WiiUMenuApp::pauseSpotifyForLocal() {
    if (m_switify.nowPlaying().playing)
        m_switify.send(sw::IpcCommand::Pause);
}

void WiiUMenuApp::setMusicSource(bool spotify) {
    if (spotifySource() == spotify) return;
    if (spotify)
        pauseLocalForSpotify();
    else
        pauseSpotifyForLocal();
    m_config.musicSource = spotify ? 1 : 0;
    m_config.save();
    m_spotifyBrowseRoot = true;
    DebugLog::log("[music] source=%s", spotify ? "spotify" : "local");
    auto& i18n = nxui::I18n::instance();
    m_accessibility.announce(spotify
        ? i18n.tr("quicksettings.source_spotify", "Spotify")
        : i18n.tr("quicksettings.source_local", "Songs"));
    m_musicLinkPollIn = 0.f;
}

void WiiUMenuApp::requestSwitifyPlayback() {
    const auto now = m_switify.nowPlaying();
    auto& i18n = nxui::I18n::instance();
    auto showInfo = [this](const std::string& title, const std::string& text) {
        if (!m_dialog) return;
        m_dialogReturnFocus = focusManager().current();
        m_dialog->show(title, text,
            {{nxui::I18n::instance().tr("button.ok", "OK"), []() {}, true}});
        focusManager().setFocus(m_dialog.get());
    };
    switch (now.state) {
        case sw::ServiceState::NotInstalled:
            showInfo(i18n.tr("switify.not_installed", "Switify is not installed"),
                i18n.tr("switify.not_installed_desc",
                    "Install the Switify overlay and its background service to play Spotify here."));
            return;
        case sw::ServiceState::WaitingForCredentials:
            showInfo(i18n.tr("switify.sign_in", "Sign in with the Switify app"),
                i18n.tr("switify.sign_in_desc",
                    "Open the Switify app once and sign in to Spotify. The background service reuses that session."));
            return;
        case sw::ServiceState::NotRunning:
            if (!m_dialog) return;
            m_dialogReturnFocus = focusManager().current();
            m_dialog->show(i18n.tr("switify.start_title", "Start Switify?"),
                i18n.tr("switify.start_desc",
                    "Switify's background service will start and keep playing Spotify while you play. You can stop it from the Switify overlay."),
                {{i18n.tr("button.cancel", "Cancel"), []() {}, true},
                 {i18n.tr("switify.start", "Start"), [this]() {
                     m_switify.launchService();
                     m_musicLinkPollIn = 0.f;
                     m_switifyAutoPlay = true;
                     m_switifyAutoPlayElapsed = 0.f;
                     m_switifyAutoPlayRetry = 0.f;
                 }, true}}, 1, {});
            focusManager().setFocus(m_dialog.get());
            return;
        case sw::ServiceState::Starting:
        case sw::ServiceState::Connecting:
            return;
        case sw::ServiceState::Ready:
            if (!now.playing) pauseLocalForSpotify();
            m_switify.send(sw::IpcCommand::PlayPause);
            return;
    }
}

void WiiUMenuApp::toggleActiveMusic() {
    if (spotifySource()) {
        requestSwitifyPlayback();
        return;
    }
    m_audio.togglePlayPause();
    if (m_audio.isPlaying()) pauseSpotifyForLocal();
    m_config.musicEnabled = m_audio.isPlaying();
    persistMusicPlaybackState(false);
    m_config.save();
}

void WiiUMenuApp::stepActiveMusic(bool next) {
    if (spotifySource()) {
        if (m_switify.nowPlaying().controllable())
            m_switify.send(next ? sw::IpcCommand::Next : sw::IpcCommand::Previous);
        return;
    }
    if (next) m_audio.nextTrack(); else m_audio.prevTrack();
    pauseSpotifyForLocal();
    persistMusicPlaybackState(false);
    m_config.musicEnabled = true;
    m_config.save();
}

void WiiUMenuApp::setSpotifyVolume(float volume) {
    volume = std::clamp(volume, 0.f, 1.f);
    m_spotifyPendingVolume = volume;
    m_spotifyPendingVolumeAt = std::chrono::steady_clock::now();
    m_switify.send(sw::IpcCommand::SetVolume,
                   static_cast<std::uint32_t>(volume * 1000.f + 0.5f));
}

QuickSettingsOverlay::MusicUiState WiiUMenuApp::spotifyMusicState() {
    auto& i18n = nxui::I18n::instance();
    const auto now = m_switify.nowPlaying();
    QuickSettingsOverlay::MusicUiState state;
    state.spotify = true;
    state.currentIndex = -1;
    if (now.controllable() && now.hasTrack)
        state.nowPlaying = now.artist.empty() ? now.title : now.artist + " - " + now.title;
    else if (now.state == sw::ServiceState::NotRunning)
        state.nowPlaying = switifyStatusText(now) + " · "
            + i18n.tr("switify.press_play", "Press Play to start it");
    else
        state.nowPlaying = switifyStatusText(now);
    state.playing = now.playing;
    state.shuffle = now.shuffle;
    state.repeat = static_cast<MusicRepeatMode>(now.repeat);
    state.positionSeconds = static_cast<float>(now.positionMs) / 1000.f;
    state.durationSeconds = now.canSeek || now.durationMs > 0
        ? static_cast<float>(now.durationMs) / 1000.f : 0.f;
    if (m_spotifyPendingVolume >= 0.f &&
        std::chrono::steady_clock::now() - m_spotifyPendingVolumeAt < kPendingVolumeHold)
        state.volume = m_spotifyPendingVolume;
    else
        state.volume = now.controllable()
            ? static_cast<float>(now.volumePermille) / 1000.f : m_config.musicVolume;
    if (const auto cover = m_switify.cover())
        state.coverKey = cover->key;
    state.playlistHeader = i18n.tr("quicksettings.source_spotify", "Spotify");
    if (!now.controllable())
        return state;

    auto addRow = [&state](std::string title, std::string key) {
        state.playlistTitles.push_back(std::move(title));
        state.playlistOrderKeys.push_back(std::move(key));
    };
    if (m_spotifyBrowseRoot) {
        addRow(i18n.tr("switify.queue", "Queue"), "root:queue");
        addRow(i18n.tr("switify.current_playlist", "Current playlist"), "root:playlist");
        addRow(i18n.tr("switify.library", "Your Library"), "root:library");
        addRow(i18n.tr("switify.home", "Home"), "root:home");
        addRow(i18n.tr("switify.search", "Search..."), "root:search");
        return state;
    }

    const auto page = m_switify.browsePage();
    if (page.state == sw::kBrowseIdle) {
        // Nothing requested yet (or the service restarted): back to the menu.
        m_spotifyBrowseRoot = true;
        return spotifyMusicState();
    }
    if (!page.title.empty()) state.playlistHeader = page.title;
    const std::string generation = std::to_string(page.generation);
    if (page.state == sw::kBrowseLoading) {
        addRow(i18n.tr("switify.loading", "Loading..."), "loading:" + generation);
    } else if (page.state == sw::kBrowseError) {
        addRow(page.message.empty()
                   ? i18n.tr("switify.load_failed", "Could not load this page")
                   : page.message,
               "error:" + generation);
    } else {
        for (std::size_t i = 0; i < page.items.size(); ++i) {
            const auto& item = page.items[i];
            std::string title;
            if (item.track) {
                title = item.subtitle.empty() ? item.name : item.name + " - " + item.subtitle;
            } else {
                title = "▶ " + item.name;
                if (!item.detail.empty()) title += "  (" + item.detail + ")";
                else if (!item.subtitle.empty()) title += "  (" + item.subtitle + ")";
            }
            addRow(std::move(title), generation + ":" + std::to_string(i));
        }
    }
    return state;
}

void WiiUMenuApp::activateSpotifyRow(int index) {
    if (!m_switify.nowPlaying().controllable()) {
        requestSwitifyPlayback();
        return;
    }
    if (m_spotifyBrowseRoot) {
        if (index == RootSearch) {
            auto& i18n = nxui::I18n::instance();
            requestTextEntry(i18n.tr("switify.search_title", "Search Spotify"),
                i18n.tr("switify.search_title", "Search Spotify"), {}, 100, false,
                [this](const std::string& query) {
                    if (query.empty()) return;
                    m_spotifyBrowseRoot = false;
                    m_switify.browse(sw::IpcCommand::BrowseSearch, 0, 0, query);
                });
            return;
        }
        static constexpr sw::IpcCommand kRootCommands[] = {
            sw::IpcCommand::BrowseQueue, sw::IpcCommand::BrowseCurrentPlaylist,
            sw::IpcCommand::BrowseLibrary, sw::IpcCommand::BrowseHome};
        if (index < 0 || index >= RootSearch) return;
        m_spotifyBrowseRoot = false;
        m_switify.browse(kRootCommands[index]);
        return;
    }

    const auto page = m_switify.browsePage();
    if (page.state != sw::kBrowseReady || index < 0 ||
        index >= static_cast<int>(page.items.size()))
        return;
    const auto position = static_cast<std::uint32_t>(index);
    if (page.items[static_cast<std::size_t>(index)].track) {
        pauseLocalForSpotify();
        m_switify.browse(sw::IpcCommand::BrowsePlay, page.generation, position);
    } else {
        m_switify.browse(sw::IpcCommand::BrowseOpen, page.generation, position);
    }
}

bool WiiUMenuApp::spotifyBrowseBack() {
    if (!spotifySource() || m_spotifyBrowseRoot) return false;
    const auto page = m_switify.browsePage();
    if (page.state == sw::kBrowseLoading || page.depth == 0) {
        m_spotifyBrowseRoot = true;
        return true;
    }
    m_switify.browse(sw::IpcCommand::BrowseBack, page.generation);
    return true;
}

WiiUMenuApp::NowPlayingView WiiUMenuApp::currentNowPlayingView() const {
    auto& i18n = nxui::I18n::instance();
    NowPlayingView view;
    const std::string playingLabel = i18n.tr("widget.playing", "Playing");
    const std::string pausedLabel = i18n.tr("widget.paused", "Paused");
    if (spotifySource()) {
        const auto now = m_switify.nowPlaying();
        view.header = i18n.tr("quicksettings.source_spotify", "Spotify");
        if (now.controllable() && now.hasTrack) {
            view.header += " · " + (now.playing ? playingLabel : pausedLabel);
            view.primary = now.title;
            view.secondary = now.artist;
            view.playing = now.playing;
            view.hasTrack = true;
            setProgress(view, static_cast<float>(now.positionMs) / 1000.f,
                        static_cast<float>(now.durationMs) / 1000.f);
            if (const auto cover = m_switify.cover())
                view.coverKey = cover->key;
        } else {
            view.primary = switifyStatusText(now);
            if (now.state == sw::ServiceState::NotRunning)
                view.secondary = i18n.tr("switify.press_a", "Press A to start it");
        }
        return view;
    }

    view.header = i18n.tr("quicksettings.source_local", "Songs");
    const int index = m_audio.currentIndex();
    const auto& tracks = m_audio.tracks();
    if (index < 0 || index >= static_cast<int>(tracks.size())) {
        view.primary = i18n.tr("quicksettings.music_empty", "No music loaded");
        return view;
    }
    const auto& track = tracks[static_cast<std::size_t>(index)];
    view.header += " · " + (m_audio.isPlaying() ? playingLabel : pausedLabel);
    view.primary = track.title.empty() ? track.displayTitle() : track.title;
    view.secondary = track.title.empty() ? std::string() : track.artist;
    view.coverKey = "local:" + m_audio.currentTrackKey();
    view.playing = m_audio.isPlaying();
    view.hasTrack = true;
    setProgress(view, m_audio.positionSeconds(), m_audio.durationSeconds());
    return view;
}

void WiiUMenuApp::applyNowPlayingWidget(GlossyIcon& icon) const {
    icon.setWidgetHeader(m_nowPlayingView.header);
    icon.setWidgetText(m_nowPlayingView.primary, m_nowPlayingView.secondary);
    icon.setWidgetCover(m_nowPlayingCover.get());
    icon.setWidgetBars(m_musicBars.data(), static_cast<int>(m_musicBars.size()));
    icon.setWidgetPlayback(m_nowPlayingView.progress, m_nowPlayingView.playing,
                           m_nowPlayingView.time);
}

void WiiUMenuApp::applyPlaylistShelfWidget(GlossyIcon& icon) const {
    auto& i18n = nxui::I18n::instance();
    const auto& shelf = m_playlistShelf;
    icon.setWidgetHeader(i18n.tr("widget.playlists_header", "Spotify · Your playlists"));
    const auto now = m_switify.nowPlaying();
    if (!now.controllable()) {
        icon.setWidgetText(switifyStatusText(now),
                           now.state == sw::ServiceState::NotRunning
                               ? i18n.tr("switify.press_a", "Press A to start it") : std::string());
    } else if (shelf.names.empty()) {
        icon.setWidgetText(shelf.listed && shelf.step == PlaylistShelf::Step::Idle
                               ? i18n.tr("widget.playlists_empty", "No playlists found")
                               : i18n.tr("widget.playlists_loading", "Loading your library..."),
                           i18n.tr("widget.playlists_retry", "Plus to try again"));
    } else {
        const auto index = static_cast<std::size_t>(shelf.index);
        std::string detail = std::to_string(shelf.index + 1) + " / " +
                             std::to_string(shelf.names.size());
        detail += shelf.step == PlaylistShelf::Step::Idle
            ? " · " + i18n.tr("widget.playlists_a", "A to play")
            : " · " + i18n.tr("widget.playlists_starting", "Starting...");
        if (!shelf.details[index].empty()) detail = shelf.details[index] + " · " + detail;
        icon.setWidgetText(shelf.names[index], detail);
    }
    icon.setWidgetPlayback(0.f, false, {});
}

void WiiUMenuApp::refreshPlaylistShelfWidgets() {
    if (!m_grid) return;
    for (const auto& icon : m_grid->allIcons()) {
        if (icon && icon->entryKind() == GridEntryKind::Widget &&
            icon->widgetType() == switchu::widgets::WidgetType::SpotifyPlaylists)
            applyPlaylistShelfWidget(*icon);
    }
}

void WiiUMenuApp::stepPlaylistShelf(int delta) {
    auto& shelf = m_playlistShelf;
    if (shelf.names.empty()) {
        shelf.listed = false; // ask the library again
    } else {
        const int count = static_cast<int>(shelf.names.size());
        shelf.index = ((shelf.index + delta) % count + count) % count;
        m_accessibility.announce(shelf.names[static_cast<std::size_t>(shelf.index)]);
    }
    refreshPlaylistShelfWidgets();
}

void WiiUMenuApp::playPlaylistShelf() {
    auto& shelf = m_playlistShelf;
    if (!m_switify.nowPlaying().controllable()) {
        requestSwitifyPlayback(); // starting the service asks first
        return;
    }
    if (shelf.names.empty()) {
        shelf.listed = false;
        refreshPlaylistShelfWidgets();
        return;
    }
    if (shelf.step != PlaylistShelf::Step::Idle) return;
    if (!spotifySource()) setMusicSource(true);
    shelf.target = shelf.names[static_cast<std::size_t>(shelf.index)];
    shelf.step = PlaylistShelf::Step::OpeningLibrary;
    shelf.waited = 0.f;
    m_switify.browse(sw::IpcCommand::BrowseLibrary);
    DebugLog::log("[playlists] play '%s'", shelf.target.c_str());
    refreshPlaylistShelfWidgets();
}

void WiiUMenuApp::pollPlaylistShelf() {
    auto& shelf = m_playlistShelf;
    const auto& widgets = m_widgetStore.all();
    if (std::none_of(widgets.begin(), widgets.end(), [](const auto& widget) {
            return widget.type == switchu::widgets::WidgetType::SpotifyPlaylists;
        }))
        return;
    const bool browsingByHand = m_quickSettings && m_quickSettings->isActive();
    const auto now = m_switify.nowPlaying();
    if (!now.controllable()) {
        shelf.step = PlaylistShelf::Step::Idle;
        return;
    }
    if (shelf.step == PlaylistShelf::Step::Idle) {
        // The list, once per session (Plus asks again), never under the user's
        // own browsing in Quick Settings.
        if (!shelf.listed && !browsingByHand) {
            shelf.listed = true;
            shelf.step = PlaylistShelf::Step::Listing;
            shelf.waited = 0.f;
            m_switify.browse(sw::IpcCommand::BrowseLibrary);
            refreshPlaylistShelfWidgets();
        }
        return;
    }

    shelf.waited += kMusicLinkPeriod;
    const auto page = m_switify.browsePage();
    const auto finish = [this, &shelf](const char* why) {
        if (why) DebugLog::log("[playlists] %s", why);
        shelf.step = PlaylistShelf::Step::Idle;
        m_spotifyBrowseRoot = true; // Quick Settings opens at its own root
        refreshPlaylistShelfWidgets();
    };
    if (page.state == sw::kBrowseError || shelf.waited > 20.f) {
        finish(page.state == sw::kBrowseError ? "browse failed" : "browse timed out");
        return;
    }
    if (page.state != sw::kBrowseReady) return;

    const auto isCollection = [](const sw::BrowseItem& item) {
        return !item.track && item.kind != sw::kBrowseKindSection;
    };
    switch (shelf.step) {
        case PlaylistShelf::Step::Listing:
        case PlaylistShelf::Step::OpeningLibrary: {
            if (page.depth != 0) return;
            std::vector<std::string> names, details;
            int found = -1;
            for (std::size_t i = 0; i < page.items.size(); ++i) {
                const auto& item = page.items[i];
                if (!isCollection(item)) continue;
                if (item.name == shelf.target) found = static_cast<int>(i);
                names.push_back(item.name);
                details.push_back(item.detail.empty() ? item.subtitle : item.detail);
            }
            // Keep the one on show where it was, if the library moved.
            const std::string shown = shelf.names.empty()
                ? std::string() : shelf.names[static_cast<std::size_t>(shelf.index)];
            shelf.names = std::move(names);
            shelf.details = std::move(details);
            shelf.index = 0;
            for (std::size_t i = 0; i < shelf.names.size(); ++i)
                if (shelf.names[i] == shown) shelf.index = static_cast<int>(i);
            if (shelf.step == PlaylistShelf::Step::Listing) {
                DebugLog::log("[playlists] library has %zu", shelf.names.size());
                finish(nullptr);
                return;
            }
            if (found < 0) {
                finish("playlist gone from the library");
                return;
            }
            shelf.step = PlaylistShelf::Step::OpeningList;
            shelf.waited = 0.f;
            m_switify.browse(sw::IpcCommand::BrowseOpen, page.generation,
                             static_cast<std::uint32_t>(found));
            return;
        }
        case PlaylistShelf::Step::OpeningList: {
            if (page.depth == 0) return;
            std::vector<std::uint32_t> tracks;
            for (std::size_t i = 0; i < page.items.size(); ++i)
                if (page.items[i].track) tracks.push_back(static_cast<std::uint32_t>(i));
            if (tracks.empty()) {
                finish("playlist has no tracks");
                return;
            }
            // With shuffle on, not always its first song.
            const std::uint32_t pick = now.shuffle
                ? tracks[static_cast<std::size_t>(std::rand()) % tracks.size()] : tracks.front();
            pauseLocalForSpotify();
            m_switify.browse(sw::IpcCommand::BrowsePlay, page.generation, pick);
            finish(nullptr);
            return;
        }
        case PlaylistShelf::Step::Idle:
            return;
    }
}

void WiiUMenuApp::refreshNowPlayingWidgets(const NowPlayingView& view) {
    if (!m_grid) return;
    auto forEachWidget = [this](auto&& fn) {
        for (const auto& icon : m_grid->allIcons()) {
            if (icon && icon->entryKind() == GridEntryKind::Widget &&
                icon->widgetType() == switchu::widgets::WidgetType::NowPlaying)
                fn(*icon);
        }
    };

    if (view.coverKey != m_nowPlayingCoverKey) {
        std::unique_ptr<nxui::Texture> cover;
        if (view.coverKey.rfind("local:", 0) == 0) {
            // ponytail: local covers decode here (as Quick Settings already
            // does); they only change on a track change inside HOME.
            const std::vector<uint8_t>* bytes = &m_audio.currentCoverArt();
            const int index = m_audio.currentIndex();
            if (bytes->empty() && index >= 0 && index < m_audio.trackCount())
                bytes = m_audio.folderCoverArt(
                    m_audio.tracks()[static_cast<std::size_t>(index)].albumFolder);
            if (bytes && !bytes->empty()) {
                cover = std::make_unique<nxui::Texture>();
                if (!cover->loadFromMemory(app().gpu(), app().renderer(), bytes->data(),
                                           bytes->size(), sw::Client::kCoverDecodeSide))
                    cover.reset();
            }
        } else if (const auto decoded = m_switify.cover();
                   decoded && decoded->key == view.coverKey && !decoded->rgba.empty()) {
            // Decoded on the Switify worker; only the upload happens here.
            cover = std::make_unique<nxui::Texture>();
            if (!cover->loadFromPixels(app().gpu(), app().renderer(), decoded->rgba.data(),
                                       decoded->width, decoded->height))
                cover.reset();
        }
        if (m_nowPlayingCover) {
            // The previous frame may still sample the old cover.
            app().gpu().waitIdle();
            forEachWidget([](GlossyIcon& icon) { icon.setWidgetCover(nullptr); });
            if (m_musicChip) m_musicChip->setCover(nullptr);
            m_nowPlayingCover.reset();
#ifdef NXUI_BACKEND_DEKO3D
            app().renderer().reclaimReleasedTextureSlotsAfterIdle();
#endif
        }
        m_nowPlayingCover = std::move(cover);
        m_nowPlayingCoverKey = view.coverKey;

        // The record on the album lock screen wears the album's two main
        // colours. Switify's covers come decoded; a song's own cover is decoded
        // once for it, and only when that lock screen can show it. Without a
        // cover, two colours of the song's own.
        m_lockDiscSeed = static_cast<std::uint32_t>(std::hash<std::string>{}(view.coverKey));
        const auto hueColour = [](float degrees) {
            const float hue = std::fmod(degrees, 360.f) / 60.f;
            const float c = 0.75f, x = c * (1.f - std::fabs(std::fmod(hue, 2.f) - 1.f));
            const float rgb[6][3] = {{c, x, 0}, {x, c, 0}, {0, c, x}, {0, x, c}, {x, 0, c}, {c, 0, x}};
            const auto& h = rgb[static_cast<int>(hue) % 6];
            return nxui::Color(0.2f + h[0], 0.2f + h[1], 0.2f + h[2], 1.f);
        };
        m_lockDiscColor = hueColour(static_cast<float>(m_lockDiscSeed % 360u));
        m_lockDiscColor2 = hueColour(static_cast<float>(m_lockDiscSeed % 360u) + 150.f);
        if (const auto decoded = m_switify.cover();
            decoded && decoded->key == view.coverKey && !decoded->rgba.empty()) {
            coverColours(decoded->rgba.data(), decoded->rgba.size() / 4,
                         m_lockDiscColor, m_lockDiscColor2);
        } else if (view.coverKey.rfind("local:", 0) == 0 &&
                   (m_config.lockAlbumSources & kMusicSourceSongs)) {
            const std::vector<uint8_t>* bytes = &m_audio.currentCoverArt();
            const int index = m_audio.currentIndex();
            if (bytes->empty() && index >= 0 && index < m_audio.trackCount())
                bytes = m_audio.folderCoverArt(
                    m_audio.tracks()[static_cast<std::size_t>(index)].albumFolder);
            int width = 0, height = 0, channels = 0;
            if (bytes && !bytes->empty()) {
                if (stbi_uc* pixels = stbi_load_from_memory(bytes->data(), static_cast<int>(bytes->size()),
                                                            &width, &height, &channels, 4)) {
                    coverColours(pixels, static_cast<std::size_t>(width) * height,
                                 m_lockDiscColor, m_lockDiscColor2);
                    stbi_image_free(pixels);
                }
            }
        }
    }
    m_nowPlayingView = view;
    forEachWidget([this](GlossyIcon& icon) { applyNowPlayingWidget(icon); });
    if (m_musicChip) {
        m_musicChip->setCover(m_nowPlayingCover.get());
        m_musicChip->setTrack(view.primary, view.secondary, view.playing);
    }
    placeMusicChip();
}

void WiiUMenuApp::resumeSpotifyAfterSleep() {
    if (!m_spotifyPausedForSleep) return;
    m_spotifyPausedForSleep = false;
    DebugLog::log("[power] resuming spotify");
    m_spotifyReconnecting = 45.f;
    // Nursing the old stream back kept failing in new ways. Switify waits for the
    // connection and starts the same song afresh, where it was.
    m_switify.send(sw::IpcCommand::ReloadCurrent);
}

void WiiUMenuApp::updateLockMusic() {
    bool hasTrack = false;
    m_lockMusicPlaying = false;
    if (lockMusicOn()) {
        // Only with a track: starting the service asks through a dialog the lock
        // would cover.
        if (spotifySource()) {
            const auto now = m_switify.nowPlaying();
            hasTrack = now.controllable() && now.hasTrack;
            // Reconnecting after a wake counts as playing: the lock's idle rule
            // put the console back to sleep mid-reconnect, and the song stayed off.
            m_lockMusicPlaying = hasTrack && (now.playing || now.reloading ||
                                              m_spotifyReconnecting > 0.f);
        } else if (lockAllowsLocalMusic()) {
            const int index = m_audio.currentIndex();
            hasTrack = index >= 0 && index < m_audio.trackCount();
            m_lockMusicPlaying = hasTrack && m_audio.isPlaying();
        }
    }
    m_lockMusicShown = hasTrack && (m_lockMusicPlaying || m_lockMusicKeep ||
                                    (m_spotifyReconnecting > 0.f && spotifySource()));
    // Bars and shapes move; otherwise only the clock does.
    app().setIdlePresentStride(m_lockMusicShown || (m_config.lockShapes && !m_lockGameBackdrop) ? 0 : 6);
}

void WiiUMenuApp::updateMusicBars(float dt) {
    bool onScreen = m_lockScreenActive ? m_lockMusicShown : false;
    if (!m_lockScreenActive && m_grid && m_appLayoutMode == AppLayoutMode::Grid) {
        const auto& icons = m_grid->allIcons();
        const int perPage = std::max(1, m_grid->iconsPerPage());
        const int start = m_grid->currentPage() * perPage;
        const int end = std::min(start + perPage, static_cast<int>(icons.size()));
        for (int i = start; i < end && !onScreen; ++i)
            onScreen = icons[static_cast<std::size_t>(i)]->entryKind() == GridEntryKind::Widget
                && icons[static_cast<std::size_t>(i)]->widgetType()
                    == switchu::widgets::WidgetType::NowPlaying;
    }
    if (!m_lockScreenActive && m_musicChip && m_musicChip->isVisible())
        onScreen = true;
    if (m_screenOff)
        onScreen = false;

    m_spotifyReconnecting = std::max(0.f, m_spotifyReconnecting - dt);
    // Slowly, while it plays.
    if (m_lockScreenActive && m_lockMusicPlaying)
        m_lockDiscAngle = std::fmod(m_lockDiscAngle + dt * 1.9f, 6.2831853f); // ~18 rpm
    const bool local = !spotifySource();
    const bool playing = onScreen && (local ? m_audio.isPlaying() : m_switify.nowPlaying().playing);
    m_audio.setSpectrumEnabled(onScreen && local);
    // Spotify's audio is in Switify's process, so it measures it there. The
    // console's output recorder is not an option: it froze the console.
    m_switify.setLevelsWanted(playing && !local);
    if (!onScreen) {
        m_musicBars.fill(0.f);
        return;
    }

    static_assert(sw::Client::kLevelBands == SpectrumAnalyzer::kBands);
    float levels[SpectrumAnalyzer::kBands]{};
    if (local)
        m_audio.spectrum(levels);
    else
        m_switify.levels(levels);
    // Sound means it is back, unless Switify says it still waits: then the sound
    // is what was loaded before the sleep.
    if (!m_switify.nowPlaying().reloading &&
        std::any_of(std::begin(levels), std::end(levels), [](float v) { return v > 0.01f; }))
        m_spotifyReconnecting = 0.f;
    // Up quickly, down slowly.
    for (int i = 0; i < SpectrumAnalyzer::kBands; ++i) {
        float& v = m_musicBars[static_cast<std::size_t>(i)];
        const float target = playing ? levels[i] : 0.f;
        v = target > v ? v + (target - v) * 0.6f : std::max(target, v - dt * 1.4f);
    }
}

bool WiiUMenuApp::lockAlbumStyle() const {
    // A password's prompt and dots need the middle: classic there.
    return (m_config.lockAlbumSources & activeMusicSource()) != 0 && m_lockMusicShown &&
           !(usingLockPassword() && m_lockTask != LockTask::Preview);
}

void WiiUMenuApp::renderLockMusic(nxui::Renderer& ren, float y, float a, bool album) {
    constexpr float kW = 1280.f;
    constexpr float kMargin = 44.f;
    constexpr float kRowTop = 622.f;
    constexpr float kRowGap = 30.f;
    const nxui::Color ink(1.f, 1.f, 1.f, a);
    const nxui::Color soft(1.f, 1.f, 1.f, 0.72f * a);

    if (album) {
        // The sleeve in the middle, a record half out of it. The record is
        // the album's two main colours, split, so its turning shows.
        constexpr float kSide = 290.f;
        const nxui::Rect art{kW * 0.5f - kSide * 0.72f, 300.f - kSide * 0.5f + y, kSide, kSide};
        const float discR = kSide * 0.47f;
        const nxui::Vec2 dc{art.x + kSide * 0.5f + kSide * 0.44f, art.y + kSide * 0.5f};
        const nxui::Color one = m_lockDiscColor, two = m_lockDiscColor2;
        // The album's colour as a soft glow behind it all.
        ren.drawCircle({kW * 0.5f, art.y + kSide * 0.5f}, kSide * 0.95f, one.withAlpha(0.06f * a), 72);
        ren.drawCircle({kW * 0.5f, art.y + kSide * 0.5f}, kSide * 0.75f, two.withAlpha(0.06f * a), 72);
        ren.drawCircle({dc.x + 4.f, dc.y + 8.f}, discR, nxui::Color(0.f, 0.f, 0.f, 0.40f * a), 72);
        // Two halves, turning: a fan of thin slices each.
        constexpr int kSlices = 36;
        constexpr float kPi = 3.14159265f;
        for (int i = 0; i < kSlices * 2; ++i) {
            const float t0 = m_lockDiscAngle + kPi * static_cast<float>(i) / kSlices;
            const float t1 = t0 + kPi / kSlices;
            const nxui::Color& c = i < kSlices ? one : two;
            ren.drawTriangle(dc, {dc.x + std::cos(t0) * discR, dc.y + std::sin(t0) * discR},
                             {dc.x + std::cos(t1) * discR, dc.y + std::sin(t1) * discR},
                             nxui::Color(c.r * 0.85f, c.g * 0.85f, c.b * 0.85f, a));
        }
        // Grooves pressed in: faint dark rings, the song's own spacing.
        const int rings = 5 + static_cast<int>(m_lockDiscSeed % 3u);
        for (int ring = 0; ring < rings; ++ring) {
            const float r = discR * (0.93f - 0.095f * static_cast<float>(ring)
                                     - static_cast<float>((m_lockDiscSeed >> (ring * 3)) % 4u) * 0.006f);
            ren.drawRoundedRectOutline({dc.x - r, dc.y - r, r * 2.f, r * 2.f},
                                       nxui::Color(0.f, 0.f, 0.f, 0.20f * a), r, 1.2f);
        }
        // The lamp's reflection stays put while the record turns under it.
        for (const float centre : {-2.25f, 0.89f}) {
            constexpr int kWedge = 6;
            constexpr float kSpread = 0.40f;
            for (int i = 0; i < kWedge; ++i) {
                const float t0 = centre - kSpread + kSpread * 2.f * static_cast<float>(i) / kWedge;
                const float t1 = t0 + kSpread * 2.f / kWedge;
                const float edge = 1.f - std::fabs((static_cast<float>(i) + 0.5f) / kWedge - 0.5f) * 2.f;
                const nxui::Color light(1.f, 1.f, 1.f, 0.10f * edge * a);
                const auto at = [&](float t, float r) {
                    return nxui::Vec2{dc.x + std::cos(t) * discR * r, dc.y + std::sin(t) * discR * r};
                };
                ren.drawTriangle(at(t0, 0.34f), at(t0, 0.97f), at(t1, 0.97f), light);
                ren.drawTriangle(at(t0, 0.34f), at(t1, 0.97f), at(t1, 0.34f), light);
            }
        }
        ren.drawRoundedRectOutline({dc.x - discR, dc.y - discR, discR * 2.f, discR * 2.f},
                                   nxui::Color(1.f, 1.f, 1.f, 0.12f * a), discR, 1.5f);
        // The label: paper white, ringed in the first colour, a mark in the
        // second riding round with it, then the spindle hole.
        ren.drawCircle(dc, discR * 0.30f, one.withAlpha(a), 48);
        ren.drawCircle(dc, discR * 0.26f, nxui::Color(0.94f, 0.93f, 0.90f, a), 48);
        ren.drawCircle({dc.x + std::cos(m_lockDiscAngle) * discR * 0.17f,
                        dc.y + std::sin(m_lockDiscAngle) * discR * 0.17f},
                       discR * 0.04f, two.withAlpha(a), 16);
        ren.drawCircle(dc, discR * 0.035f, nxui::Color(0.05f, 0.05f, 0.06f, a), 16);

        // The sleeve over it, with a shadow.
        ren.drawRoundedRect({art.x + 6.f, art.y + 10.f, art.width, art.height},
                            nxui::Color(0.f, 0.f, 0.f, 0.40f * a), 14.f);
        if (m_nowPlayingCover && m_nowPlayingCover->valid())
            ren.drawTextureRounded(m_nowPlayingCover.get(), art, 12.f, ink);
        else
            ren.drawRoundedRect(art, one.withAlpha(0.6f * a), 12.f);
    }

    // The bars, between the date and the dots; a password's prompt and dots sit
    // lower, so there they stay above it (look from HomeNX). Under the cover in
    // the album style.
    constexpr float kBarsW = 620.f;
    const float kBarsBase = album ? 552.f
        : (usingLockPassword() && m_lockTask != LockTask::Preview ? 392.f : 490.f);
    constexpr int kBands = SpectrumAnalyzer::kBands;
    const float step = kBarsW / static_cast<float>(kBands);
    const float barW = step * 0.6f;
    // (The official Switify has no levels: no bars rather than flat ones.)
    const bool hasBars = !spotifySource() || m_switify.levelsAvailable();
    for (int i = 0; i < kBands && hasBars; ++i) {
        const float v = m_musicBars[static_cast<std::size_t>(i)];
        const float h = 3.f + (album ? 48.f : 60.f) * v;
        const float x = (kW - kBarsW) * 0.5f + step * static_cast<float>(i) + step * 0.2f;
        ren.drawRoundedRect({x, kBarsBase + y - h, barW, h},
                            nxui::Color(1.f, 1.f, 1.f, (0.35f + 0.5f * v) * a), barW * 0.5f);
        ren.drawRoundedRect({x, kBarsBase + y + 4.f, barW, h * 0.3f},
                            nxui::Color(1.f, 1.f, 1.f, 0.12f * a), barW * 0.5f);
    }

    // Top left, blinking: over the bars it hid them.
    if (m_spotifyReconnecting > 0.f && spotifySource()) {
        const std::string waiting = nxui::I18n::instance().tr(
            "lockscreen.music_reconnecting", "Reconnecting...");
        // The countdown runs every frame, so it doubles as the blink's clock.
        const float blink = 0.5f + 0.5f * std::cos(m_spotifyReconnecting * 2.f * 3.14159265f);
        ren.drawText(waiting, {kMargin, (album ? 118.f : 28.f) + y}, &m_fontSmall,
                     nxui::Color(1.f, 1.f, 1.f, (0.25f + 0.65f * blink) * a));
    }

    // The song, bottom right: name and band, then the cover.
    constexpr float kCover = 56.f;
    const nxui::Rect cover{kW - kMargin - kCover, kRowTop + y, kCover, kCover};
    if (!album) {
        if (m_nowPlayingCover && m_nowPlayingCover->valid())
            ren.drawTextureRounded(m_nowPlayingCover.get(), cover, 8.f, ink);
        else
            ren.drawRoundedRect(cover, nxui::Color(1.f, 1.f, 1.f, 0.10f * a), 8.f);
    }
    constexpr float kTextW = 300.f;
    const float textRight = album ? kW - kMargin : cover.x - 12.f;
    const auto songLine = [&](const std::string& text, float top, const nxui::Color& color) {
        const float width = m_fontSmall.measure(text).x;
        const float scale = width > kTextW ? std::max(0.6f, kTextW / width) : 1.f;
        ren.drawText(text, {textRight - width * scale, top}, &m_fontSmall, color, scale);
    };
    // Long names are shrunk first and only then clipped.
    ren.pushClipRect({textRight - kTextW, kRowTop + y, kTextW, kCover});
    songLine(m_nowPlayingView.primary, kRowTop + y, ink);
    songLine(m_nowPlayingView.secondary, kRowTop + y + kRowGap, soft);
    ren.popClipRect();

    // The controls, bottom left.
    if (m_lockTask == LockTask::Unlock) {
        auto& i18n = nxui::I18n::instance();
        ren.drawText(i18n.tr("lockscreen.music_skip", "L/R: Previous/Next"),
                     {kMargin, kRowTop + y}, &m_fontSmall, soft);
        ren.drawText(i18n.tr("lockscreen.music_play", "ZR: Play/Pause"),
                     {kMargin, kRowTop + y + kRowGap}, &m_fontSmall, soft);
    }
}

void WiiUMenuApp::pollMusicLink(float dt) {
    m_switify.start();
    // Pages are only read while someone browses: Quick Settings, or the
    // playlists widget walking to a playlist.
    m_switify.setDemand((m_quickSettings && m_quickSettings->isActive()) ||
                                m_playlistShelf.step != PlaylistShelf::Step::Idle
        ? sw::Client::Demand::Active : sw::Client::Demand::Background);

    m_musicLinkPollIn -= dt;
    if (m_musicLinkPollIn > 0.f) return;
    m_musicLinkPollIn = kMusicLinkPeriod;

    // Linked sources: Spotify starting (from here or from the Switify
    // overlay) takes over, so HOME's BGM never plays on top of it.
    const auto now = m_switify.nowPlaying();
    if (now.playing && !m_spotifyWasPlaying) {
        pauseLocalForSpotify();
        if (!spotifySource()) {
            m_config.musicSource = 1;
            m_config.save();
            m_spotifyBrowseRoot = true;
            DebugLog::log("[music] spotify started; source=spotify");
        }
    }
    m_spotifyWasPlaying = now.playing;
    pollPlaylistShelf();

    if (m_switifyAutoPlay) {
        constexpr float kGiveUpSeconds = 30.f;
        constexpr float kRetrySeconds = 2.f; // Ready can come before the player can start
        m_switifyAutoPlayElapsed += kMusicLinkPeriod;
        m_switifyAutoPlayRetry -= kMusicLinkPeriod;
        if (now.playing || m_switifyAutoPlayElapsed >= kGiveUpSeconds) {
            DebugLog::log("[music] spotify autoplay done playing=%d", now.playing ? 1 : 0);
            m_switifyAutoPlay = false;
        } else if (now.controllable() && m_switifyAutoPlayRetry <= 0.f) {
            m_switifyAutoPlayRetry = kRetrySeconds;
            pauseLocalForSpotify();
            m_switify.send(sw::IpcCommand::Play); // does nothing if already playing
        }
    }

#ifdef SWITCHU_MENU
    // Staying awake for the music is the opposite of what Battery Saver is for.
    const bool screenOffWanted = !m_config.batterySaver
        && ((now.playing && (m_config.screenOffMusicSources & kMusicSourceSpotify)) ||
            (m_audio.isPlaying() && (m_config.screenOffMusicSources & kMusicSourceSongs)));
    const int linkState = (screenOffWanted ? 1 : 0) | (now.playing ? 2 : 0);
    if (linkState != m_screenOffWantedSent) {
        m_screenOffWantedSent = linkState;
        switchu::menu::smi_cmd::setScreenOffMusic(screenOffWanted, now.playing);
    }
    // The system's own auto-sleep (Settings, Sleep) waits while music plays.
    // It belongs to this applet: it ends with HOME, and a game keeps its own.
    const bool keepAwake = m_config.musicKeepsAwake &&
        (now.playing || now.reloading || m_audio.isPlaying());
    if (keepAwake != m_autoSleepDisabled) {
        m_autoSleepDisabled = keepAwake;
        const Result rc = appletSetAutoSleepDisabled(keepAwake);
        // Media playback also keeps the system from dimming the screen.
        const Result rcMedia = appletSetMediaPlaybackState(keepAwake);
        DebugLog::log("[power] auto-sleep %s rc=0x%X media=0x%X", keepAwake ? "off (music)" : "on",
                      rc, rcMedia);
    }
    // Dark and silent is the worst of both: a short grace for a skip, then real sleep.
    constexpr float kDarkSilenceSeconds = 20.f;
    m_screenOffSilence = m_screenOff && !screenOffWanted
        ? m_screenOffSilence + kMusicLinkPeriod : 0.f;
    if (m_screenOffSilence >= kDarkSilenceSeconds) {
        m_screenOffSilence = 0.f;
        DebugLog::log("[power] dark and silent: sleeping");
        m_launcher.enterSleep();
    }
#endif

    // Always: the music tile in the top bar shows it too.
    const NowPlayingView view = currentNowPlayingView();
    if (!(view == m_nowPlayingView) || view.coverKey != m_nowPlayingCoverKey)
        refreshNowPlayingWidgets(view);
}
