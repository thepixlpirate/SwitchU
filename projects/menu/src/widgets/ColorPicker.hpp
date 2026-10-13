#pragma once

#include <nxui/widgets/GlassWidget.hpp>
#include <nxui/core/Animation.hpp>
#include <nxui/core/Font.hpp>
#include <nxui/core/Input.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/Theme.hpp>

#include <functional>
#include <string>

// Small centred colour chooser: a wheel of swatches (hue around, lightness
// inward) plus a hex code. D-pad left/right turns around the wheel, up/down
// moves between rings; X types a hex code; A applies; B cancels.
class ColorPicker final : public nxui::GlassWidget {
public:
    using HexCallback = std::function<void(const std::string& hex)>;
    using VoidCallback = std::function<void()>;

    ColorPicker();

    void setFont(nxui::Font* font) { m_font = font; }
    void setSmallFont(nxui::Font* font) { m_smallFont = font; }
    void setTheme(const nxui::Theme* theme) { m_theme = theme; }
    void onApply(HexCallback cb) { m_onApply = std::move(cb); }
    void onClosed(VoidCallback cb) { m_onClosed = std::move(cb); }
    void onTypeHex(VoidCallback cb) { m_onTypeHex = std::move(cb); }
    void onNavigate(VoidCallback cb) { m_onNavigate = std::move(cb); }

    // `preview` decides how the sample is drawn: an outlined cover or a
    // filled screen-shaped swatch.
    enum class Preview { Outline, Fill };
    void open(const std::string& title, const std::string& hex, Preview preview);
    void close();
    bool isActive() const { return m_active; }
    // Sets the current colour from a typed hex code (keeps the picker open).
    bool setHex(const std::string& hex);
    std::string hex() const;

    void handleTouch(nxui::Input& input);
    void update(float dt) override;
    void render(nxui::Renderer& renderer) override;

    static bool parseHex(const std::string& text, nxui::Color& out);
    static std::string toHex(const nxui::Color& color);

private:
    static constexpr int kHues = 24;
    static constexpr int kRings = 4;   // ring 0 = outer
    nxui::Color swatchColor(int ring, int hue) const;
    nxui::Vec2 swatchCenter(int ring, int hue) const;
    float swatchRadius(int ring) const;
    void moveSelection(int dRing, int dHue);
    void selectNearest(const nxui::Color& color);

    nxui::Font* m_font = nullptr;
    nxui::Font* m_smallFont = nullptr;
    const nxui::Theme* m_theme = nullptr;
    bool m_active = false;
    std::string m_title;
    Preview m_preview = Preview::Outline;
    // ring -1 = the centre swatch (white); otherwise ring/hue on the wheel.
    int m_ring = 0;
    int m_hue = 0;
    // A typed colour that is not on the wheel.
    bool m_custom = false;
    nxui::Color m_customColor{1.f, 1.f, 1.f, 1.f};
    nxui::AnimatedFloat m_reveal{0.f};
    HexCallback m_onApply;
    VoidCallback m_onClosed;
    VoidCallback m_onTypeHex;
    VoidCallback m_onNavigate;
};
