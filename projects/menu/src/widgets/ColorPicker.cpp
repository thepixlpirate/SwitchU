#include "ColorPicker.hpp"

#include <nxui/core/I18n.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace {

constexpr float kPi = 3.14159265f;
const nxui::Rect kPanel{250.f, 70.f, 780.f, 580.f};
const nxui::Vec2 kWheelCenter{kPanel.x + 260.f, kPanel.y + 310.f};
// Tappable buttons under the preview.
const nxui::Rect kApplyButton{kPanel.x + 540.f, kPanel.y + 480.f, 200.f, 48.f};
const nxui::Rect kCancelButton{kPanel.x + 540.f, kPanel.y + 400.f, 200.f, 48.f};
const nxui::Rect kHexButton{kPanel.x + 540.f, kPanel.y + 330.f, 200.f, 48.f};
constexpr float kRingRadius[] = {205.f, 160.f, 116.f, 74.f};   // outer -> inner
constexpr float kSwatchRadius[] = {24.f, 20.f, 15.f, 10.f};

nxui::Color hsv(float h, float s, float v) {
    h = std::fmod(h, 1.f) * 6.f;
    const int i = static_cast<int>(std::floor(h));
    const float f = h - static_cast<float>(i);
    const float p = v * (1.f - s);
    const float q = v * (1.f - s * f);
    const float t = v * (1.f - s * (1.f - f));
    switch (i % 6) {
        case 0: return {v, t, p, 1.f};
        case 1: return {q, v, p, 1.f};
        case 2: return {p, v, t, 1.f};
        case 3: return {p, q, v, 1.f};
        case 4: return {t, p, v, 1.f};
        default: return {v, p, q, 1.f};
    }
}

float distance2(const nxui::Color& a, const nxui::Color& b) {
    const float dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

} // namespace

ColorPicker::ColorPicker() {
    setVisible(false);
    setFocusable(true);
    setFrameworkTouchEnabled(false);
    setTag("color_picker");
    setCornerRadius(26.f);
    setLiquidGlassEnabled(false);
    setForceLiquidGlass(false);
    setLiquidGlassShaderEnabled(false);
    setBlurEnabled(false);
    setPanelOpacity(0.98f);

    addDirectionAction(nxui::FocusDirection::LEFT, [this]() { moveSelection(0, -1); });
    addDirectionAction(nxui::FocusDirection::RIGHT, [this]() { moveSelection(0, 1); });
    addDirectionAction(nxui::FocusDirection::UP, [this]() { moveSelection(-1, 0); });
    addDirectionAction(nxui::FocusDirection::DOWN, [this]() { moveSelection(1, 0); });
    addAction(static_cast<std::uint64_t>(nxui::Button::A), [this]() { applyAndClose(); });
    addAction(static_cast<std::uint64_t>(nxui::Button::B), [this]() { close(); });
    addAction(static_cast<std::uint64_t>(nxui::Button::X), [this]() {
        if (m_active && m_onTypeHex) m_onTypeHex();
    });
}

bool ColorPicker::parseHex(const std::string& text, nxui::Color& out) {
    std::string digits;
    for (char c : text)
        if (std::isxdigit(static_cast<unsigned char>(c))) digits.push_back(c);
    if (digits.size() == 3) {   // #RGB -> #RRGGBB
        digits = {digits[0], digits[0], digits[1], digits[1], digits[2], digits[2]};
    }
    if (digits.size() != 6) return false;
    unsigned value = 0;
    if (std::sscanf(digits.c_str(), "%x", &value) != 1) return false;
    out = {((value >> 16) & 0xFF) / 255.f, ((value >> 8) & 0xFF) / 255.f,
           (value & 0xFF) / 255.f, 1.f};
    return true;
}

std::string ColorPicker::toHex(const nxui::Color& color) {
    auto byte = [](float v) { return static_cast<unsigned>(std::lround(std::clamp(v, 0.f, 1.f) * 255.f)); };
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", byte(color.r), byte(color.g), byte(color.b));
    return buf;
}

nxui::Color ColorPicker::swatchColor(int ring, int hue) const {
    if (ring < 0) return {1.f, 1.f, 1.f, 1.f};   // centre: white
    const float h = static_cast<float>(hue) / kHues;
    switch (ring) {
        case 0: return hsv(h, 1.f, 0.55f);   // outer: deep
        case 1: return hsv(h, 1.f, 1.f);     // pure
        case 2: return hsv(h, 0.6f, 1.f);    // soft
        default: return hsv(h, 0.3f, 1.f);   // inner: pastel
    }
}

nxui::Vec2 ColorPicker::swatchCenter(int ring, int hue) const {
    if (ring < 0) return kWheelCenter;
    const float angle = -kPi * 0.5f + 2.f * kPi * static_cast<float>(hue) / kHues;
    const float r = kRingRadius[ring];
    return {kWheelCenter.x + std::cos(angle) * r, kWheelCenter.y + std::sin(angle) * r};
}

float ColorPicker::swatchRadius(int ring) const {
    return ring < 0 ? 26.f : kSwatchRadius[ring];
}

std::string ColorPicker::hex() const {
    return toHex(m_custom ? m_customColor : swatchColor(m_ring, m_hue));
}

void ColorPicker::selectNearest(const nxui::Color& color) {
    float best = distance2(color, swatchColor(-1, 0));
    m_ring = -1;
    m_hue = 0;
    for (int ring = 0; ring < kRings; ++ring) {
        for (int hue = 0; hue < kHues; ++hue) {
            const float d = distance2(color, swatchColor(ring, hue));
            if (d < best) { best = d; m_ring = ring; m_hue = hue; }
        }
    }
    // Keep the exact colour when it is not one of the swatches.
    m_custom = best > 0.0004f;
    m_customColor = color;
}

bool ColorPicker::setHex(const std::string& text) {
    nxui::Color color;
    if (!parseHex(text, color)) return false;
    selectNearest(color);
    return true;
}

void ColorPicker::open(const std::string& title, const std::string& hexValue, Preview preview) {
    m_title = title;
    m_preview = preview;
    nxui::Color color;
    if (parseHex(hexValue, color)) selectNearest(color);
    else { m_ring = 1; m_hue = 0; m_custom = false; }
    m_active = true;
    setVisible(true);
    setRect(kPanel);
    if (m_theme) {
        setBaseColor(m_theme->panelBase.withAlpha(0.98f));
        setBorderColor(m_theme->panelBorder.withAlpha(0.5f));
        setHighlightColor(m_theme->panelHighlight.withAlpha(0.12f));
    }
    m_reveal.setImmediate(0.9f);
    m_reveal.set(1.f, 0.18f, nxui::Easing::outBack);
}

void ColorPicker::close() {
    if (!m_active) return;
    m_active = false;
    setVisible(false);
    if (m_onClosed) m_onClosed();
}

void ColorPicker::moveSelection(int dRing, int dHue) {
    if (!m_active) return;
    m_custom = false;
    if (m_ring < 0) {
        // From the centre: any direction steps onto the inner ring.
        m_ring = kRings - 1;
        if (dHue != 0) m_hue = (m_hue + dHue + kHues) % kHues;
    } else if (dRing != 0) {
        const int next = m_ring + dRing;   // up = outward, down = inward
        if (next >= kRings) m_ring = -1;   // past the inner ring: the centre
        else m_ring = std::max(0, next);
    } else {
        m_hue = (m_hue + dHue + kHues) % kHues;
    }
    if (m_onNavigate) m_onNavigate();
}

void ColorPicker::applyAndClose() {
    if (!m_active) return;
    auto cb = m_onApply;
    const std::string value = hex();
    close();
    if (cb) cb(value);
}

void ColorPicker::handleTouch(nxui::Input& input) {
    if (!m_active || !input.touchUp()) return;
    const float x = input.touchX(), y = input.touchY();
    if (!kPanel.contains(x, y)) { close(); return; }
    if (kApplyButton.contains(x, y)) { applyAndClose(); return; }
    if (kCancelButton.contains(x, y)) { close(); return; }
    if (kHexButton.contains(x, y)) { if (m_onTypeHex) m_onTypeHex(); return; }
    auto hit = [&](int ring, int hue) {
        const nxui::Vec2 c = swatchCenter(ring, hue);
        const float r = swatchRadius(ring) + 4.f;
        return (x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= r * r;
    };
    if (hit(-1, 0)) { m_ring = -1; m_custom = false; if (m_onNavigate) m_onNavigate(); return; }
    for (int ring = 0; ring < kRings; ++ring)
        for (int hue = 0; hue < kHues; ++hue)
            if (hit(ring, hue)) {
                m_ring = ring; m_hue = hue; m_custom = false;
                if (m_onNavigate) m_onNavigate();
                return;
            }
}

void ColorPicker::update(float dt) {
    if (!m_active) return;
    m_reveal.update(dt);
    nxui::GlassWidget::update(dt);
}

void ColorPicker::render(nxui::Renderer& ren) {
    if (!m_active || !isVisible()) return;
    const float alpha = std::clamp(m_reveal.value(), 0.f, 1.f);
    ren.drawRect({0.f, 0.f, 1280.f, 720.f}, nxui::Color(0.f, 0.f, 0.f, 0.55f * alpha));
    setRect(kPanel);
    nxui::GlassWidget::render(ren);

    const nxui::Color primary = m_theme ? m_theme->textPrimary : nxui::Color::white();
    const nxui::Color secondary = m_theme ? m_theme->textSecondary
                                          : nxui::Color(0.78f, 0.82f, 0.88f, 1.f);
    if (m_font)
        ren.drawText(m_title, {kPanel.x + 30.f, kPanel.y + 22.f}, m_font,
                     primary.withAlpha(alpha), 0.8f);

    // Wheel.
    for (int ring = 0; ring < kRings; ++ring) {
        for (int hue = 0; hue < kHues; ++hue) {
            ren.drawCircle(swatchCenter(ring, hue), swatchRadius(ring),
                           swatchColor(ring, hue).withAlpha(alpha), 24);
        }
    }
    ren.drawCircle(swatchCenter(-1, 0), swatchRadius(-1), swatchColor(-1, 0).withAlpha(alpha), 32);

    // Selection ring (not drawn for a typed colour that is off the wheel).
    if (!m_custom) {
        const nxui::Vec2 c = swatchCenter(m_ring, m_hue);
        const float r = swatchRadius(m_ring) + 5.f;
        ren.drawCircle(c, r + 3.f, nxui::Color(0.f, 0.f, 0.f, 0.7f * alpha), 32);
        ren.drawCircle(c, r, nxui::Color::white().withAlpha(alpha), 32);
        ren.drawCircle(c, swatchRadius(m_ring),
                       swatchColor(m_ring, m_hue).withAlpha(alpha), 32);
    }

    // Preview + hex on the right.
    const nxui::Color chosen = m_custom ? m_customColor : swatchColor(m_ring, m_hue);
    const float px = kPanel.x + 540.f;
    if (m_preview == Preview::Outline) {
        const nxui::Rect card{px + 30.f, kPanel.y + 90.f, 140.f, 210.f};
        ren.drawRoundedRect(card, nxui::Color(0.2f, 0.2f, 0.24f, alpha), 14.f);
        ren.drawRoundedRectOutline(card.expanded(6.f), chosen.withAlpha(alpha), 20.f, 6.f);
    } else {
        const nxui::Rect screen{px, kPanel.y + 160.f, 200.f, 112.f};
        ren.drawRoundedRect(screen.expanded(3.f), secondary.withAlpha(0.6f * alpha), 12.f);
        ren.drawRoundedRect(screen, chosen.withAlpha(alpha), 10.f);
    }
    // Buttons (tap them, or use X / B / A).
    auto& i18n = nxui::I18n::instance();
    nxui::Font* bf = m_smallFont ? m_smallFont : m_font;
    auto button = [&](const nxui::Rect& r, const std::string& label, bool primaryButton) {
        ren.drawRoundedRect(r, primaryButton ? chosen.withAlpha(alpha)
                                             : secondary.withAlpha(0.25f * alpha), 14.f);
        ren.drawRoundedRectOutline(r, secondary.withAlpha(0.6f * alpha), 14.f, 1.5f);
        if (!bf) return;
        const float s = 0.7f;
        const nxui::Vec2 m = bf->measure(label);
        // Dark text on light colours, white text on dark ones.
        const float lum = 0.299f * chosen.r + 0.587f * chosen.g + 0.114f * chosen.b;
        const nxui::Color text = primaryButton
            ? (lum > 0.6f ? nxui::Color(0.08f, 0.08f, 0.1f, alpha) : nxui::Color::white().withAlpha(alpha))
            : primary.withAlpha(alpha);
        ren.drawText(label, {r.x + (r.width - m.x * s) * 0.5f, r.y + (r.height - m.y * s) * 0.5f},
                     bf, text, s);
    };
    button(kHexButton, hex() + "   (X)", false);
    button(kCancelButton, i18n.tr("color.cancel", "Cancel  (B)"), false);
    button(kApplyButton, i18n.tr("color.apply", "Apply  (A)"), true);
    if (m_smallFont)
        ren.drawText(i18n.tr("color.help_move", "D-pad: pick a colour"),
                     {kPanel.x + 30.f, kPanel.y + kPanel.height - 34.f}, m_smallFont,
                     secondary.withAlpha(alpha), 0.6f);
}
