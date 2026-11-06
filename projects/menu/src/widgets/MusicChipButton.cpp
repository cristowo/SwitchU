#include "MusicChipButton.hpp"

#include <nxui/core/I18n.hpp>

#include <algorithm>

MusicChipButton::MusicChipButton() {
    setTag("musicChip");
    setFocusable(true);
    setCornerRadius(26.f);
    setPadding(6.f);
    setLiquidGlassEnabled(true);
    setForceLiquidGlass(true);
    setBlurEnabled(false);
    setPanelOpacity(0.90f);
    setBorderWidth(1.8f);
    setBaseColor(nxui::Color(0.20f, 0.22f, 0.28f, 0.94f));
    setBorderColor(nxui::Color::white().withAlpha(0.24f));
    setHighlightColor(nxui::Color::white().withAlpha(0.08f));
    setTrack({}, {}, false);
}

// The same look as the profile tile beside it.
void MusicChipButton::setTheme(const nxui::Theme* theme) {
    m_theme = theme;
    if (!theme)
        return;
    setBaseColor(theme->iconDefault.withAlpha(theme->mode == nxui::ThemeMode::Dark ? 0.92f : 0.94f));
    setBorderColor(theme->panelBorder);
    setHighlightColor(theme->panelHighlight);
}

void MusicChipButton::setTrack(const std::string& title, const std::string& artist, bool playing) {
    m_playing = playing;
    auto& i18n = nxui::I18n::instance();
    m_statusText = title.empty() ? i18n.tr("widget.now_playing", "Now playing")
                                 : (artist.empty() ? title : artist + " - " + title);
    setAccessibilityLabel(m_statusText);
    setAccessibilityRole(i18n.tr("accessibility.roles.music", "music"));
    setAccessibilityHint(i18n.tr("musicchip.hint", "A for the music controls."));
}

void MusicChipButton::onContentRender(nxui::Renderer& ren) {
    const float alpha = opacity();
    nxui::Rect art = glassContentRect();
    const float side = std::min(art.width, art.height);
    art.x += (art.width - side) * 0.5f;
    art.y += (art.height - side) * 0.5f;
    art.width = side;
    art.height = side;
    const float radius = std::max(0.f, cornerRadius() - padding().top);

    if (m_cover && m_cover->valid()) {
        ren.drawTextureRounded(m_cover, art, radius, nxui::Color::white().withAlpha(alpha));
    } else {
        ren.drawRoundedRect(art, m_theme ? m_theme->panelBase.withAlpha(0.55f * alpha)
                                         : nxui::Color(0.42f, 0.42f, 0.50f, 0.42f * alpha),
                            radius);
        // A note: its head and stem.
        const nxui::Color glyph = m_theme ? m_theme->textSecondary.withAlpha(0.75f * alpha)
                                          : nxui::Color::white().withAlpha(0.75f * alpha);
        const float u = side / 10.f;
        ren.drawCircle({art.x + 4.2f * u, art.y + 6.6f * u}, 1.6f * u, glyph, 20);
        ren.drawRect({art.x + 5.3f * u, art.y + 2.4f * u, 0.7f * u, 4.3f * u}, glyph);
        ren.drawRoundedRect({art.x + 5.3f * u, art.y + 2.4f * u, 2.4f * u, 1.0f * u}, glyph, 0.5f * u);
    }

    if (m_bars && m_barCount > 0) {
        // Every other band: a small tile has room for half of them.
        const int shown = m_barCount / 2;
        const float step = (side - 8.f) / static_cast<float>(shown);
        const float barWidth = step * 0.62f;
        for (int i = 0; i < shown; ++i) {
            const float level = std::max(m_bars[i * 2], m_bars[i * 2 + 1]);
            if (level < 0.01f) continue;
            const float height = 1.5f + side * 0.36f * level;
            const nxui::Rect bar{art.x + 4.f + step * static_cast<float>(i) + step * 0.19f,
                                 art.bottom() - 4.f - height, barWidth, height};
            ren.drawRoundedRect({bar.x + 1.f, bar.y + 1.f, bar.width, bar.height},
                                nxui::Color(0.f, 0.f, 0.f, 0.30f * alpha), barWidth * 0.5f);
            ren.drawRoundedRect(bar, nxui::Color::white().withAlpha((0.6f + 0.35f * level) * alpha),
                                barWidth * 0.5f);
        }
    }

    if (!m_playing) {
        const float badge = side * 0.42f;
        const nxui::Vec2 c{art.right() - badge * 0.5f - 2.f, art.y + badge * 0.5f + 2.f};
        ren.drawCircle(c, badge * 0.5f, nxui::Color(0.f, 0.f, 0.f, 0.55f * alpha));
        const float barH = badge * 0.42f;
        const float barW = badge * 0.14f;
        for (float dx : {-badge * 0.13f, badge * 0.13f})
            ren.drawRoundedRect({c.x + dx - barW * 0.5f, c.y - barH * 0.5f, barW, barH},
                                nxui::Color::white().withAlpha(0.95f * alpha), barW * 0.5f);
    }
}
