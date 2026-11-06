#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>

// Entering or making the lock screen password: 4 to 8 D-pad presses, as the
// letters U, D, L and R. Making one asks for it twice. No file or hash here,
// the caller passes the check, so this runs in a host test. (From HomeNX.)
class LockPasswordEntry {
public:
    static constexpr std::size_t kMin = 4;
    static constexpr std::size_t kMax = 8;

    enum class Mode { Unlock, Make, Again };
    enum class Result { None, Unlocked, Wrong, Made, Mismatch, TooShort };

    void startUnlock(std::size_t length, std::function<bool(const std::string&)> check) {
        m_mode = Mode::Unlock;
        m_want = length;
        m_check = std::move(check);
        m_presses.clear();
        m_failure = Result::None;
        m_shake = 0.f;
    }

    void startMaking() {
        m_mode = Mode::Make;
        m_presses.clear();
        m_first.clear();
        m_failure = Result::None;
        m_shake = 0.f;
    }

    Result press(char direction) {
        if (m_presses.size() >= kMax)
            return Result::None;
        m_presses += direction;
        m_pop = 1.f;
        if (m_mode == Mode::Unlock && m_presses.size() == m_want) {
            const bool ok = m_check && m_check(m_presses);
            m_presses.clear();
            return ok ? Result::Unlocked : fail(Result::Wrong);
        }
        if (m_mode == Mode::Again && m_presses.size() == m_first.size()) {
            const bool same = m_presses == m_first;
            m_presses.clear();
            if (same)
                return Result::Made;
            startMaking();
            return fail(Result::Mismatch);
        }
        return Result::None;
    }

    // A while making it: what was pressed so far is the password.
    Result confirm() {
        if (m_mode != Mode::Make)
            return Result::None;
        if (m_presses.size() < kMin)
            return fail(Result::TooShort);
        m_first = m_presses;
        m_presses.clear();
        m_mode = Mode::Again;
        return Result::None;
    }

    void clear() { m_presses.clear(); }

    void update(float dt) {
        m_shake = std::max(0.f, m_shake - dt * 1.4f);
        m_pop = std::max(0.f, m_pop - dt * 5.f);
    }

    Mode mode() const { return m_mode; }
    const std::string& presses() const { return m_presses; }
    const std::string& made() const { return m_first; }
    float shake() const { return m_shake; }
    float pop() const { return m_pop; }
    // The failure to show while the dots still shake; None otherwise.
    Result failure() const { return m_shake > 0.f ? m_failure : Result::None; }
    std::size_t dots() const {
        if (m_mode == Mode::Unlock) return m_want;
        if (m_mode == Mode::Again) return m_first.size();
        return std::max(m_presses.size(), kMin);
    }

private:
    Result fail(Result why) {
        m_failure = why;
        m_shake = 1.f;
        return why;
    }

    Mode m_mode = Mode::Unlock;
    std::size_t m_want = 0;
    std::string m_presses;
    std::string m_first;
    std::function<bool(const std::string&)> m_check;
    Result m_failure = Result::None;
    float m_shake = 0.f;
    float m_pop = 0.f;
};
