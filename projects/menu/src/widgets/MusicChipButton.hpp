#pragma once

#include <nxui/widgets/GlassWidget.hpp>
#include <nxui/core/Texture.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/Theme.hpp>

#include <string>

// HOME's top bar, beside the profile tile: what is playing, drawn like the 1x1
// "Now playing" widget (the cover, the bars over its foot, a pause badge).
// Activating it opens Quick Settings on the music controls.
class MusicChipButton : public nxui::GlassWidget {
public:
    MusicChipButton();

    void setTheme(const nxui::Theme* theme);
    // Non-owning: the menu owns the cover and the bars, and clears the cover
    // here before replacing it.
    void setCover(const nxui::Texture* cover) { m_cover = cover; }
    void setBars(const float* levels, int count) { m_bars = levels; m_barCount = count; }
    void setTrack(const std::string& title, const std::string& artist, bool playing);
    const std::string& statusText() const { return m_statusText; }

protected:
    void onContentRender(nxui::Renderer& ren) override;

private:
    const nxui::Theme* m_theme = nullptr;
    const nxui::Texture* m_cover = nullptr;
    const float* m_bars = nullptr;
    int m_barCount = 0;
    bool m_playing = false;
    std::string m_statusText;
};
