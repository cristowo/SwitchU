#include "core/WidgetStore.hpp"

#include <array>
#include <cassert>
#include <string>

using switchu::widgets::WidgetSize;
using switchu::widgets::WidgetType;

int main() {
    constexpr std::array types{
        WidgetType::Clock,
        WidgetType::RecentlyPlayed,
        WidgetType::RecentPlaytime,
        WidgetType::RandomScreenshot,
        WidgetType::ImagePin,
        WidgetType::Batteries,
        WidgetType::NowPlaying,
        WidgetType::Calendar,
        WidgetType::SpotifyPlaylists,
    };

    for (const WidgetType type : types) {
        const auto singleRow = switchu::widgets::supportedSizes(
            type, AppLayoutMode::DynamicLine);
        if (type == WidgetType::RecentlyPlayed || type == WidgetType::RecentPlaytime ||
            type == WidgetType::Batteries || type == WidgetType::NowPlaying ||
            type == WidgetType::RandomScreenshot || type == WidgetType::Calendar ||
            type == WidgetType::SpotifyPlaylists) {
            assert(singleRow.empty());
            assert((switchu::widgets::validatedSize(
                        type, {2, 1}, AppLayoutMode::DynamicLine) == WidgetSize{0, 0}));
        } else {
            assert(singleRow.size() == 1);
            assert((singleRow.front() == WidgetSize{1, 1}));
            assert((switchu::widgets::validatedSize(
                        type, {2, 2}, AppLayoutMode::DynamicLine) == WidgetSize{1, 1}));
        }
    }

    assert(switchu::widgets::supportedSizes(
               WidgetType::Clock, AppLayoutMode::Grid).size() == 2);
    const auto recentlyPlayed = switchu::widgets::supportedSizes(
        WidgetType::RecentlyPlayed, AppLayoutMode::Grid);
    assert(recentlyPlayed.size() == 1);
    assert((recentlyPlayed.front() == WidgetSize{2, 1}));
    assert(switchu::widgets::supportedSizes(
               WidgetType::RandomScreenshot, AppLayoutMode::Grid).empty());
    assert((switchu::widgets::supportedSizes(
                WidgetType::RecentPlaytime, AppLayoutMode::Grid) ==
            std::vector<WidgetSize>{{2, 1}}));
    assert((switchu::widgets::supportedSizes(
                WidgetType::Batteries, AppLayoutMode::Grid) ==
            std::vector<WidgetSize>{{2, 1}, {2, 2}}));
    assert((switchu::widgets::validatedSize(
                WidgetType::Clock, {2, 2}, AppLayoutMode::Grid) == WidgetSize{1, 1}));
    assert((switchu::widgets::validatedSize(
                WidgetType::ImagePin, {1, 2}, AppLayoutMode::Grid) == WidgetSize{1, 2}));

    assert((switchu::widgets::validatedSize(
                WidgetType::Calendar, {2, 1}, AppLayoutMode::Grid) == WidgetSize{2, 2}));
    WidgetType parsedCalendar = WidgetType::Clock;
    assert(switchu::widgets::parseType(
        switchu::widgets::typeKey(WidgetType::Calendar), parsedCalendar));
    assert(parsedCalendar == WidgetType::Calendar);
    assert(switchu::widgets::calendarDaysInMonth(2028, 2) == 29);
    assert(switchu::widgets::calendarDaysInMonth(2100, 2) == 28);
    assert(switchu::widgets::calendarDaysInMonth(2000, 2) == 29);
    assert(switchu::widgets::calendarDaysInMonth(2026, 11) == 30);
    // 2 Nov 2026 is a Monday, so the 1st is a Sunday.
    assert(switchu::widgets::calendarLeadingBlanks(2, 1, 0) == 0);
    assert(switchu::widgets::calendarLeadingBlanks(2, 1, 1) == 6);
    // 31 Jan 2026 is a Saturday, so the 1st is a Thursday.
    assert(switchu::widgets::calendarLeadingBlanks(31, 6, 0) == 4);
    assert(switchu::widgets::calendarLeadingBlanks(31, 6, 1) == 3);

    for (const std::uint32_t id : {1u, 42u, 0xFFFFFFFFu}) {
        const auto titleId = switchu::widgets::widgetTitleId(id);
        assert(switchu::widgets::isWidgetTitleId(titleId));
        assert(switchu::widgets::widgetIdFromTitleId(titleId) == id);
    }
    assert(!switchu::widgets::isWidgetTitleId(0));
    assert(switchu::widgets::widgetIdFromTitleId(0) == 0);

    WidgetType parsed = WidgetType::Clock;
    assert(switchu::widgets::parseType("image_pin", parsed));
    assert(parsed == WidgetType::ImagePin);
    assert(!switchu::widgets::parseType("unknown", parsed));
    assert(switchu::widgets::parseType("now_playing", parsed));
    assert(parsed == WidgetType::NowPlaying);
    assert((switchu::widgets::supportedSizes(
                WidgetType::NowPlaying, AppLayoutMode::Grid) ==
            std::vector<WidgetSize>{{2, 1}, {1, 1}, {2, 2}}));
    // A saved size that is not offered falls back to the 2x1 card.
    assert((switchu::widgets::validatedSize(
                WidgetType::NowPlaying, {1, 2}, AppLayoutMode::Grid) == WidgetSize{2, 1}));
    assert(switchu::widgets::parseType("spotify_playlists", parsed));
    assert(parsed == WidgetType::SpotifyPlaylists);
}
