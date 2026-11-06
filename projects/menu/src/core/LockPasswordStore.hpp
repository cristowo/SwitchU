#pragma once

#include <switch.h>
#include <switchu/sd_commit.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

// The lock screen password on the SD card, as its length and a salted SHA-256.
// It only stops someone pressing buttons: there are few combinations, and
// deleting this file from a computer removes it (Settings says so). (From HomeNX.)
namespace switchu::lockpw {

inline constexpr const char* kPath = "sdmc:/config/SwitchU/lock_password";

inline std::string hash(const std::string& presses) {
    const std::string salted = "switchu-lock:" + presses;
    u8 digest[SHA256_HASH_SIZE];
    sha256CalculateHash(digest, salted.data(), salted.size());
    char hex[SHA256_HASH_SIZE * 2 + 1] = {};
    for (size_t i = 0; i < SHA256_HASH_SIZE; ++i)
        std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return hex;
}

// 0 when there is none (or the file is not one of ours).
inline size_t length() {
    std::ifstream in(kPath);
    size_t n = 0;
    std::string stored;
    in >> n >> stored;
    return stored.size() == SHA256_HASH_SIZE * 2 && n >= 4 && n <= 8 ? n : 0;
}

inline bool check(const std::string& presses) {
    std::ifstream in(kPath);
    size_t n = 0;
    std::string stored;
    in >> n >> stored;
    return n == presses.size() && stored == hash(presses);
}

inline bool save(const std::string& presses) {
    {
        std::ofstream out(kPath, std::ios::trunc);
        out << presses.size() << ' ' << hash(presses);
        out.flush();
        if (!out.good())
            return false;
    }
    return commitSdCard("lock password") && length() == presses.size();
}

inline void remove() {
    std::error_code ec;
    std::filesystem::remove(kPath, ec);
    commitSdCard("lock password removed");
}

} // namespace switchu::lockpw
