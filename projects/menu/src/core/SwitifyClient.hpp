#pragma once

#include "SwitifyProtocol.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Client for the Switify background service. Every IPC call, SD read, cover
// download and JPEG decode happens on one worker thread, so a stuck or absent
// service can never freeze HOME. The UI only reads snapshots and queues
// commands.
namespace switchu::switify {

enum class ServiceState : std::uint8_t {
    NotInstalled,
    NotRunning,
    Starting,
    WaitingForCredentials,
    Connecting,
    Ready,
};

struct NowPlaying {
    ServiceState state = ServiceState::NotRunning;
    bool hasTrack = false;
    bool playing = false;
    bool shuffle = false;
    bool canSeek = false;
    bool reloading = false; // waiting for the connection to load the song again
    int repeat = 0; // SwitchU MusicRepeatMode numbering
    std::uint32_t positionMs = 0;
    std::uint32_t durationMs = 0;
    std::uint32_t volumePermille = 0;
    std::string title;
    std::string artist;
    std::string album;

    bool controllable() const { return state == ServiceState::Ready; }
    // Everything a static display depends on (no progress/volume).
    bool sameDisplay(const NowPlaying& other) const {
        return state == other.state && hasTrack == other.hasTrack &&
               playing == other.playing && title == other.title &&
               artist == other.artist;
    }
};

struct BrowseItem {
    bool track = false;
    std::uint32_t kind = 0; // kBrowseKind*
    std::string name;
    std::string subtitle;
    std::string detail;
    std::uint32_t durationMs = 0;
    // Collections only, and only from our Switify build: what pins one.
    std::string uri;
    std::string imageUrl;
};

// What became of the last playUri().
struct PinInfo {
    std::uint32_t state = kPinIdle;
    bool current = false; // it is what plays now
    std::string uri;
    std::string name;     // as Spotify has them now (once started)
    std::string imageUrl;
};

struct BrowsePage {
    std::uint64_t generation = 0;
    std::uint32_t state = kBrowseIdle;
    std::uint32_t depth = 0;
    std::string title;
    std::string subtitle;
    std::string message;
    std::vector<BrowseItem> items;
};

struct Cover {
    std::string key;                 // artwork URL
    std::vector<std::uint8_t> bytes; // encoded JPEG
    std::vector<std::uint8_t> rgba;  // decoded, at most kCoverDecodeSide
    int width = 0;
    int height = 0;
};

class Client {
public:
    static constexpr int kCoverDecodeSide = 256;

    enum class Demand : std::uint8_t { Background, Active };

    Client() = default;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    ~Client() { stop(); }

    void start();
    void stop();
    void setDemand(Demand demand);

    // The bars. Levels need our Switify build: with the official one the first
    // request fails and levelsAvailable() turns false.
    static constexpr int kLevelBands = 32;
    void setLevelsWanted(bool wanted);
    bool levelsAvailable() const { return m_levelsAvailable.load(); }
    void levels(float* out) const;

    NowPlaying nowPlaying() const;
    BrowsePage browsePage() const;
    std::shared_ptr<const Cover> cover() const;

    void send(IpcCommand command, std::uint32_t value = 0);
    void browse(IpcCommand command, std::uint64_t generation = 0,
                std::uint32_t index = 0, std::string query = {});
    void launchService();

    // A pinned playlist (album, artist). Needs our Switify build.
    void playUri(const std::string& uri, const std::string& name);
    void setPinWatch(bool wanted) { m_pinWatch.store(wanted); }
    PinInfo pinInfo() const;
    // A picture to a file (JPEG, at most 512 KiB). Asked once; see it arrive.
    void fetchImage(std::string url, std::string path);

private:
    struct Request {
        IpcCommand command = IpcCommand::GetStatus;
        std::uint32_t value = 0;
        std::uint64_t generation = 0;
        std::uint32_t index = 0;
        std::string query;
        bool isBrowse = false;
        bool isLaunch = false;
        bool isFetch = false; // query: URL, path: the file
        std::string path;
    };

    void run();
    void queue(Request request);
    bool pendingRequests() const;
    bool connect();
    void disconnect();
    void execute(const Request& request);
    void pollStatus();
    void pollBrowse();
    void pollCover(bool hasTrack);
    void pollLevels();
    void pollPin();

    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<Demand> m_demand{Demand::Background};
    std::atomic<bool> m_levelsWanted{false};
    std::atomic<bool> m_levelsAvailable{true};
    std::array<std::uint8_t, kLevelBands> m_levels{};
    std::vector<Request> m_requests;
    NowPlaying m_now;
    BrowsePage m_browse;
    std::shared_ptr<const Cover> m_cover;
    PinInfo m_pin;
    std::atomic<bool> m_pinWatch{false};
    bool m_awaitingBrowse = false;
    std::uint64_t m_staleGeneration = 0;
    std::chrono::steady_clock::time_point m_awaitingBrowseSince{};

    // Worker-only state.
    bool m_connected = false;
    bool m_installed = false;
    std::chrono::steady_clock::time_point m_launchedAt{};
    bool m_launchPending = false;
    bool m_pinCommands = true; // false once Switify says it does not know them
    std::string m_coverUrl;
    std::string m_failedCoverUrl;
    std::chrono::steady_clock::time_point m_coverRetryAt{};
};

} // namespace switchu::switify
