#include <nxui/core/Input.hpp>
#include <algorithm>
#include <cmath>

namespace nxui {

void Input::initialize() {
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);
    padInitializeAny(&m_pad);
    hidInitializeTouchScreen();
}

void Input::shutdown() {
}

void Input::update() {
    padUpdate(&m_pad);
    m_kDown = padGetButtonsDown(&m_pad);
    m_kUp   = padGetButtonsUp(&m_pad);
    m_kHeld = padGetButtons(&m_pad);

    // Analog sticks
    HidAnalogStickState ls = padGetStickPos(&m_pad, 0);
    HidAnalogStickState rs = padGetStickPos(&m_pad, 1);
    m_lx = ls.x / 32767.f;
    m_ly = ls.y / 32767.f;
    m_rx = rs.x / 32767.f;
    m_ry = rs.y / 32767.f;

    HidTouchScreenState tState = {};
    bool hardwareTouching = hidGetTouchScreenStates(&tState, 1) && tState.count > 0;
    float hardwareTouchX = hardwareTouching ? tState.touches[0].x : m_touchX;
    float hardwareTouchY = hardwareTouching ? tState.touches[0].y : m_touchY;

    m_wasTouching = m_touchingRaw;
    m_touchingRaw = hardwareTouching;
    if (hardwareTouching) {
        m_touchX = hardwareTouchX;
        m_touchY = hardwareTouchY;
    }

    m_touching = hardwareTouching;

    m_touchDown = (m_touching && !m_wasTouching);
    m_touchUp   = (!m_touching && m_wasTouching);

    if (m_touchDown) {
        m_touchStartX = m_touchX;
        m_touchStartY = m_touchY;
        m_touchStartTick = armGetSystemTick();
    }
}

float Input::touchDuration() const {
    uint64_t now = armGetSystemTick();
    return static_cast<float>(now - m_touchStartTick) / static_cast<float>(armGetSystemTickFreq());
}

void Input::clearFrame() {
    m_kDown = m_kUp = m_kHeld = 0;
    m_lx = m_ly = m_rx = m_ry = 0.f;
    m_touching = m_touchDown = m_touchUp = false;
}

bool Input::isDown(Button b) const { return m_kDown & static_cast<uint64_t>(b); }
bool Input::isUp(Button b)   const { return m_kUp   & static_cast<uint64_t>(b); }
bool Input::isHeld(Button b) const { return m_kHeld & static_cast<uint64_t>(b); }

bool Input::hasUserActivity() const {
    constexpr float kActivityStickThreshold = 0.08f;
    return m_kDown != 0 || m_kUp != 0 || m_touching || m_touchDown || m_touchUp
        || std::abs(m_lx) >= kActivityStickThreshold
        || std::abs(m_ly) >= kActivityStickThreshold
        || std::abs(m_rx) >= kActivityStickThreshold
        || std::abs(m_ry) >= kActivityStickThreshold;
}

} // namespace nxui
