// Music sources: songs on the SD card (AudioManager) or Spotify through the
// Switify background service. Quick Settings (L), the "Now playing" widget
// and HOME's own BGM all go through these functions so that exactly one
// source plays at a time.
#include "WiiUMenuApp.hpp"
#include "widgets/GlossyIcon.hpp"
#include "DebugLog.hpp"

#include <algorithm>
#include <nxui/core/I18n.hpp>

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
    return view;
}

void WiiUMenuApp::applyNowPlayingWidget(GlossyIcon& icon) const {
    icon.setWidgetHeader(m_nowPlayingView.header);
    icon.setWidgetText(m_nowPlayingView.primary, m_nowPlayingView.secondary);
    icon.setWidgetCover(m_nowPlayingCover.get());
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
            m_nowPlayingCover.reset();
#ifdef NXUI_BACKEND_DEKO3D
            app().renderer().reclaimReleasedTextureSlotsAfterIdle();
#endif
        }
        m_nowPlayingCover = std::move(cover);
        m_nowPlayingCoverKey = view.coverKey;
    }
    m_nowPlayingView = view;
    forEachWidget([this](GlossyIcon& icon) { applyNowPlayingWidget(icon); });
}

void WiiUMenuApp::pollMusicLink(float dt) {
    m_switify.start();
    m_switify.setDemand(m_quickSettings && m_quickSettings->isActive()
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

    const auto& widgets = m_widgetStore.all();
    if (std::none_of(widgets.begin(), widgets.end(), [](const auto& widget) {
            return widget.type == switchu::widgets::WidgetType::NowPlaying;
        }))
        return;
    const NowPlayingView view = currentNowPlayingView();
    if (!(view == m_nowPlayingView) || view.coverKey != m_nowPlayingCoverKey)
        refreshNowPlayingWidgets(view);
}
