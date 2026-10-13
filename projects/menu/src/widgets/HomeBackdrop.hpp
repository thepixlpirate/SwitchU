#pragma once

#include <nxui/widgets/Widget.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Texture.hpp>

// Full-screen HOME background chosen in Themes > Options: a solid colour or
// a picture from the SD card, drawn over the theme's own background.
class HomeBackdrop final : public nxui::Widget {
public:
    enum class Mode { Theme, Solid, Image };

    HomeBackdrop() { setRect({0.f, 0.f, 1280.f, 720.f}); setFocusable(false); }

    void setMode(Mode mode) { m_mode = mode; }
    Mode mode() const { return m_mode; }
    void setColor(const nxui::Color& color) { m_color = color; }
    void setTexture(nxui::Texture* texture) { m_texture = texture; }

protected:
    void onRender(nxui::Renderer& ren) override {
        if (m_mode == Mode::Solid) {
            ren.drawRect(m_rect, m_color.withAlpha(m_opacity));
        } else if (m_mode == Mode::Image) {
            if (m_texture && m_texture->valid())
                ren.drawTexture(m_texture, m_rect, nxui::Color::white().withAlpha(m_opacity));
            else
                ren.drawRect(m_rect, nxui::Color(0.f, 0.f, 0.f, m_opacity));
        }
    }

private:
    Mode m_mode = Mode::Theme;
    nxui::Color m_color{0.f, 0.f, 0.f, 1.f};
    nxui::Texture* m_texture = nullptr;
};
