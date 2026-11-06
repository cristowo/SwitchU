#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <system_error>
#include <vector>

// Where a lock screen picture can come from. (From HomeNX.)
namespace switchu::lockpicture {

// Where the Album is: the SD card's, and emuMMC's own when it runs one (its
// emummc.ini's nintendo_path): its screenshots are there, not in sdmc:/Nintendo.
inline std::vector<std::string> albumFolders() {
    std::vector<std::string> out{"sdmc:/Nintendo/Album"};
    std::ifstream ini("sdmc:/emuMMC/emummc.ini");
    std::string line, nintendo;
    bool enabled = false;
    while (std::getline(ini, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("enabled=", 0) == 0) enabled = line.substr(8) != "0";
        if (line.rfind("nintendo_path=", 0) == 0) nintendo = line.substr(14);
    }
    while (!nintendo.empty() && nintendo.front() == '/') nintendo.erase(0, 1);
    if (enabled && !nintendo.empty()) out.push_back("sdmc:/" + nintendo + "/Album");
    return out;
}

// The newest screenshots in every Album, newest first (its folders are the date:
// year, month, day; the files start with the time).
inline std::vector<std::string> newestScreenshots(size_t count) {
    using Named = std::map<std::string, std::vector<std::string>, std::greater<>>;
    auto inside = [](const std::vector<std::string>& dirs, bool pictures) {
        Named out;
        std::error_code ec;
        for (const std::string& dir : dirs)
            for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
                if (pictures) {
                    std::string ext = e.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
                    if (!e.is_regular_file(ec) || ext != ".jpg") continue;
                } else if (!e.is_directory(ec)) {
                    continue;
                }
                out[e.path().filename().string()].push_back(e.path().string());
            }
        return out;
    };
    std::vector<std::string> shots;
    for (const auto& year : inside(albumFolders(), false))
        for (const auto& month : inside(year.second, false))
            for (const auto& day : inside(month.second, false))
                for (const auto& shot : inside(day.second, true))
                    for (const std::string& file : shot.second) {
                        if (shots.size() >= count) return shots;
                        shots.push_back(file);
                    }
    return shots;
}

} // namespace switchu::lockpicture
