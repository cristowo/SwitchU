#include "TabBuilders.hpp"
#include <nxui/core/I18n.hpp>
#include <switch.h>
#include <switchu/lock_picture.hpp>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

SettingsScreen::Tab settings::tabs::SleepTab::build(SettingsScreen& screen) {
    using Tab = SettingsScreen::Tab;
    using SettingItem = SettingsScreen::SettingItem;
    using ItemType = SettingsScreen::ItemType;
    auto& i18n = nxui::I18n::instance();
    Tab t;
    t.name = i18n.tr("settings.tabs.sleep", "Sleep");

    const std::vector<std::string> handLabels = {
        i18n.tr("settings.sleep.1min", "1 min"),
        i18n.tr("settings.sleep.3min", "3 min"),
        i18n.tr("settings.sleep.5min", "5 min"),
        i18n.tr("settings.sleep.10min", "10 min"),
        i18n.tr("settings.sleep.30min", "30 min"),
        i18n.tr("settings.sleep.never", "Never")
    };
    const std::vector<std::string> dockLabels = {
        i18n.tr("settings.sleep.1h", "1 hour"),
        i18n.tr("settings.sleep.2h", "2 hours"),
        i18n.tr("settings.sleep.3h", "3 hours"),
        i18n.tr("settings.sleep.6h", "6 hours"),
        i18n.tr("settings.sleep.12h", "12 hours"),
        i18n.tr("settings.sleep.never", "Never")
    };

    SetSysSleepSettings sleep{};
    setsysGetSleepSettings(&sleep);

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.power_actions", "Power");
        it.type = ItemType::Section;
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.sleep_now_title", "Sleep Mode");
        it.buttonLabel = i18n.tr("power.sleep", "Sleep");
        it.type = ItemType::Action;
        it.description = i18n.tr("settings.sleep.sleep_now_desc", "Put the console into sleep mode.");
        it.onChange = [&screen, &i18n](SettingItem&) {
            screen.requestDialog(
                i18n.tr("settings.sleep.sleep_now", "Sleep"),
                i18n.tr("settings.sleep.sleep_confirm", "Put the console into sleep mode?"),
                {
                    { i18n.tr("settings.sleep.sleep_now", "Sleep"), [&screen]() {
                        if (screen.m_sleepCb) screen.m_sleepCb();
                    } },
                    { i18n.tr("button.cancel", "Cancel"), []() {} }
                });
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.shutdown_title", "Power Off");
        it.buttonLabel = i18n.tr("power.shutdown", "Shutdown");
        it.type = ItemType::Action;
        it.description = i18n.tr("settings.sleep.shutdown_desc", "Completely power off the console.");
        it.onChange = [&screen, &i18n](SettingItem&) {
            screen.requestDialog(
                i18n.tr("settings.sleep.shutdown", "Shutdown"),
                i18n.tr("settings.sleep.shutdown_confirm", "Power off the console?"),
                {
                    { i18n.tr("settings.sleep.shutdown", "Shutdown"), [&screen]() {
                        if (screen.m_shutdownCb) screen.m_shutdownCb();
                    } },
                    { i18n.tr("button.cancel", "Cancel"), []() {} }
                });
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.reboot_title", "Restart Console");
        it.buttonLabel = i18n.tr("power.reboot", "Reboot");
        it.type = ItemType::Action;
        it.description = i18n.tr("settings.sleep.reboot_desc", "Restart the console.");
        it.onChange = [&screen, &i18n](SettingItem&) {
            screen.requestDialog(
                i18n.tr("settings.sleep.reboot", "Reboot"),
                i18n.tr("settings.sleep.reboot_confirm", "Restart the console?"),
                {
                    { i18n.tr("settings.sleep.reboot", "Reboot"), [&screen]() {
                        if (screen.m_rebootCb) screen.m_rebootCb();
                    } },
                    { i18n.tr("button.cancel", "Cancel"), []() {} }
                });
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.lock_screen", "Lock Screen"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.sleep.lock_screen_desc",
            "Show the lock screen when the console wakes from sleep.");
        it.boolVal = screen.m_lockScreenEnabled;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_lockScreenEnabled = self.boolVal;
            if (screen.m_lockScreenCb)
                screen.m_lockScreenCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.lock_single", "Unlock With One Press"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.sleep.lock_single_desc",
            "Press A once instead of the same button three times.");
        it.boolVal = screen.m_lockSinglePress;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_lockSinglePress = self.boolVal;
            if (screen.m_lockSinglePressCb)
                screen.m_lockSinglePressCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.lock_cascade", "Icons Fall In On Unlock"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.sleep.lock_cascade_desc",
            "The lock screen shows only the theme's background, and the icons drop into place when you unlock.");
        it.boolVal = screen.m_lockCascade;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_lockCascade = self.boolVal;
            if (screen.m_lockCascadeCb)
                screen.m_lockCascadeCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.lock_music", "Music on the Lock Screen"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.sleep.lock_music_desc",
            "While a song plays on the lock screen: bars, its name and cover, and controls on L, R and ZR. The screen stays on while it plays.");
        it.boolVal = screen.m_lockMusic;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_lockMusic = self.boolVal;
            if (screen.m_lockMusicCb)
                screen.m_lockMusicCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.screen_off_music", "Music With the Screen Off"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.sleep.screen_off_music_desc",
            "In HOME, handheld: while music plays, the power button only turns the screen off. The console stays awake and uses more battery than in sleep mode.");
        it.boolVal = screen.m_screenOffMusic;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_screenOffMusic = self.boolVal;
            if (screen.m_screenOffMusicCb)
                screen.m_screenOffMusicCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.music_keeps_awake", "No Auto-Sleep With Music"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.sleep.music_keeps_awake_desc",
            "In HOME and on the lock screen: while music plays, the console does not go to sleep by itself. It still does with the power button.");
        it.boolVal = screen.m_musicKeepsAwake;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_musicKeepsAwake = self.boolVal;
            if (screen.m_musicKeepsAwakeCb)
                screen.m_musicKeepsAwakeCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.lock_shapes", "Animated Theme on Lock Screen"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.sleep.lock_shapes_desc",
            "Shows the theme's moving shapes, like its fireflies, on the lock screen. Uses a little more battery.");
        it.boolVal = screen.m_lockShapes;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_lockShapes = self.boolVal;
            if (screen.m_lockShapesCb)
                screen.m_lockShapesCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.lock_password", "Lock Password");
        it.type = ItemType::Action;
        it.buttonLabel = screen.m_lockPasswordSet
            ? i18n.tr("button.remove", "Remove")
            : i18n.tr("button.create", "Create");
        it.description = i18n.tr("settings.sleep.lock_password_desc",
            "4 to 8 D-pad presses. Forgot it? Delete config/SwitchU/lock_password from the SD card.");
        it.onChange = [&screen](SettingItem&) {
            if (screen.m_lockPasswordCb)
                screen.m_lockPasswordCb();
        };
        t.items.push_back(std::move(it));
    }

    {
        // The newest screenshots of every Album, by date: the file names start with it.
        std::vector<std::string> paths{std::string()};
        std::vector<std::string> labels{i18n.tr("settings.sleep.lock_picture_none", "HOME, frosted")};
        for (const std::string& path : switchu::lockpicture::newestScreenshots(12)) {
            const std::string name = std::filesystem::path(path).filename().string();
            paths.push_back(path);
            labels.push_back(name.size() >= 12
                ? name.substr(0, 4) + "-" + name.substr(4, 2) + "-" + name.substr(6, 2) + " "
                    + name.substr(8, 2) + ":" + name.substr(10, 2)
                : name);
        }
        auto current = std::find(paths.begin(), paths.end(), screen.m_lockPicturePath);
        if (current == paths.end()) {
            paths.push_back(screen.m_lockPicturePath);
            labels.push_back(std::filesystem::path(screen.m_lockPicturePath).filename().string());
            current = paths.end() - 1;
        }
        SettingItem it; it.label = i18n.tr("settings.sleep.lock_picture", "Lock Picture"); it.type = ItemType::Selector;
        it.description = i18n.tr("settings.sleep.lock_picture_desc",
            "A screenshot from the Album, or what is on screen, frosted.");
        it.options = std::move(labels);
        it.intVal = static_cast<int>(current - paths.begin());
        it.onChange = [&screen, paths = std::move(paths)](SettingItem& self) {
            screen.m_lockPicturePath = paths[std::clamp(self.intVal, 0, static_cast<int>(paths.size()) - 1)];
            if (screen.m_lockPictureCb)
                screen.m_lockPictureCb(screen.m_lockPicturePath);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.lock_preview", "Preview Lock Screen");
        it.type = ItemType::Action;
        it.buttonLabel = i18n.tr("button.show", "Show");
        it.description = i18n.tr("settings.sleep.lock_preview_desc", "See the lock screen as it is set now.");
        it.onChange = [&screen](SettingItem&) {
            if (screen.m_lockPreviewCb)
                screen.m_lockPreviewCb();
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.auto_sleep", "Auto-Sleep");
        it.type = ItemType::Section;
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.handheld", "Handheld Auto-Sleep"); it.type = ItemType::Selector;
        it.description = i18n.tr("settings.sleep.handheld_desc", "Idle delay before automatic sleep in handheld mode.");
        it.options = handLabels;
        it.intVal = std::clamp((int)sleep.handheld_sleep_plan, 0, 5);
        it.onChange = [](SettingItem& self) {
            SetSysSleepSettings s{};
            setsysGetSleepSettings(&s);
            s.handheld_sleep_plan = std::clamp(self.intVal, 0, 5);
            setsysSetSleepSettings(&s);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.docked", "Docked Auto-Sleep"); it.type = ItemType::Selector;
        it.description = i18n.tr("settings.sleep.docked_desc", "Idle delay before automatic sleep while docked.");
        it.options = dockLabels;
        it.intVal = std::clamp((int)sleep.console_sleep_plan, 0, 5);
        it.onChange = [](SettingItem& self) {
            SetSysSleepSettings s{};
            setsysGetSleepSettings(&s);
            s.console_sleep_plan = std::clamp(self.intVal, 0, 5);
            setsysSetSleepSettings(&s);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.media_sleep", "Sleep While Playing Media"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.sleep.media_sleep_desc", "Allow the console to auto-sleep even during media playback.");
        bool val = (sleep.flags & (1u << 0)) != 0;
        it.boolVal = val;
        it.anim01 = val ? 1.f : 0.f;
        it.onChange = [](SettingItem& self) {
            SetSysSleepSettings s{};
            setsysGetSleepSettings(&s);
            if (self.boolVal)
                s.flags |= (1u << 0);
            else
                s.flags &= ~(1u << 0);
            setsysSetSleepSettings(&s);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.sleep.wake_power", "Wake at Power State Change"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.sleep.wake_power_desc", "Wake the console when the AC adapter is connected or disconnected.");
        bool val = (sleep.flags & (1u << 1)) != 0;
        it.boolVal = val;
        it.anim01 = val ? 1.f : 0.f;
        it.onChange = [](SettingItem& self) {
            SetSysSleepSettings s{};
            setsysGetSleepSettings(&s);
            if (self.boolVal)
                s.flags |= (1u << 1);
            else
                s.flags &= ~(1u << 1);
            setsysSetSleepSettings(&s);
        };
        t.items.push_back(std::move(it));
    }

    return t;
}
