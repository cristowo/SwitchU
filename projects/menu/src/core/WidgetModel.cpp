#include "WidgetStore.hpp"

#include <algorithm>

namespace switchu::widgets {

const char* typeKey(WidgetType type) {
    switch (type) {
        case WidgetType::Clock: return "clock";
        case WidgetType::RecentlyPlayed: return "recently_played";
        case WidgetType::RecentPlaytime: return "recent_playtime";
        case WidgetType::RandomScreenshot: return "random_screenshot";
        case WidgetType::ImagePin: return "image_pin";
        case WidgetType::Batteries: return "batteries";
        case WidgetType::NowPlaying: return "now_playing";
        case WidgetType::Calendar: return "calendar";
        case WidgetType::SpotifyPlaylists: return "spotify_playlists";
    }
    return "clock";
}

const char* fallbackName(WidgetType type) {
    switch (type) {
        case WidgetType::Clock: return "Clock";
        case WidgetType::RecentlyPlayed: return "Recently played";
        case WidgetType::RecentPlaytime: return "Recent playtime";
        case WidgetType::RandomScreenshot: return "Random screenshot";
        case WidgetType::ImagePin: return "Image pin";
        case WidgetType::Batteries: return "Batteries";
        case WidgetType::NowPlaying: return "Now playing";
        case WidgetType::Calendar: return "Calendar";
        case WidgetType::SpotifyPlaylists: return "Spotify playlists";
    }
    return "Widget";
}

bool parseType(const std::string& value, WidgetType& type) {
    for (WidgetType candidate : {WidgetType::Clock, WidgetType::RecentlyPlayed,
                                 WidgetType::RecentPlaytime, WidgetType::RandomScreenshot,
                                 WidgetType::ImagePin, WidgetType::Batteries,
                                 WidgetType::NowPlaying, WidgetType::Calendar,
                                 WidgetType::SpotifyPlaylists}) {
        if (value == typeKey(candidate)) {
            type = candidate;
            return true;
        }
    }
    return false;
}

std::vector<WidgetSize> supportedSizes(WidgetType type, AppLayoutMode layout) {
    if (layout == AppLayoutMode::DynamicLine) {
        if (type == WidgetType::RecentlyPlayed || type == WidgetType::RecentPlaytime ||
            type == WidgetType::RandomScreenshot || type == WidgetType::Batteries ||
            type == WidgetType::NowPlaying || type == WidgetType::Calendar ||
            type == WidgetType::SpotifyPlaylists)
            return {};
        return {{1, 1}};
    }
    switch (type) {
        case WidgetType::Clock:
            return {{1, 1}, {2, 1}};
        case WidgetType::RecentlyPlayed:
            // The user-facing 1x2 card is one row high and two columns wide.
            return {{2, 1}};
        case WidgetType::RecentPlaytime:
            return {{2, 1}};
        case WidgetType::RandomScreenshot:
            return {};
        case WidgetType::ImagePin:
            return {{1, 1}, {2, 1}, {1, 2}, {2, 2}};
        case WidgetType::Batteries:
            return {{2, 1}, {2, 2}};
        case WidgetType::NowPlaying:
            // 2x1 first: what a saved widget of no known size becomes.
            return {{2, 1}, {1, 1}, {2, 2}};
        case WidgetType::SpotifyPlaylists:
            return {{2, 1}};
        case WidgetType::Calendar:
            return {{2, 2}, {1, 1}};
    }
    return {{1, 1}};
}

WidgetSize validatedSize(WidgetType type, WidgetSize requested, AppLayoutMode layout) {
    const auto sizes = supportedSizes(type, layout);
    if (sizes.empty()) return {0, 0};
    const auto found = std::find(sizes.begin(), sizes.end(), requested);
    return found == sizes.end() ? sizes.front() : *found;
}

int calendarDaysInMonth(int year, int month) {
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return month == 2 && leap ? 29 : kDays[std::clamp(month, 1, 12) - 1];
}

int calendarLeadingBlanks(int monthDay, int weekday, int firstWeekday) {
    const int weekdayOfFirst = ((weekday - (monthDay - 1)) % 7 + 7) % 7;
    return (weekdayOfFirst - firstWeekday + 7) % 7;
}

} // namespace switchu::widgets
