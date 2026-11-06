#pragma once

#include <cstdint>

// Lock screen rule: the same input three times, with a short pause allowed
// between presses. Inputs are opaque ids so buttons, touch and HOME all count.
class LockPressCounter {
public:
    static constexpr int kRequiredPresses = 3;
    static constexpr float kMaxSecondsBetweenPresses = 1.2f;

    void reset() {
        m_count = 0;
        m_input = 0;
        m_sinceLastPress = 0.f;
    }

    void tick(float dt) {
        m_sinceLastPress += dt;
        if (m_count > 0 && m_sinceLastPress > kMaxSecondsBetweenPresses)
            m_count = 0;
    }

    void press(std::uint64_t input) {
        m_count = (m_count > 0 && input == m_input) ? m_count + 1 : 1;
        m_input = input;
        m_sinceLastPress = 0.f;
    }

    int count() const { return m_count; }
    bool complete() const { return m_count >= kRequiredPresses; }

private:
    int m_count = 0;
    std::uint64_t m_input = 0;
    float m_sinceLastPress = 0.f;
};
