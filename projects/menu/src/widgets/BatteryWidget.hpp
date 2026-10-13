#pragma once
#include <nxui/widgets/GlassWidget.hpp>
#include <nxui/core/Font.hpp>
#include <nxui/core/Types.hpp>


class BatteryWidget : public nxui::GlassWidget {
public:
    BatteryWidget() = default;
    void setFont(nxui::Font* f) { m_font = f; }
    void setTextColor(const nxui::Color& c) { m_textColor = c; }
    void setBatteryStatus(uint32_t percentage, bool charging);

    // Off draws only the text/icon, with no glass bubble behind it.
    void setBubbleVisible(bool visible) { m_bubbleVisible = visible; }

protected:
    void onRender(nxui::Renderer& ren) override {
        if (m_bubbleVisible) nxui::GlassWidget::onRender(ren);
        else onContentRender(ren);
    }
    void onContentUpdate(float dt) override;
    void onContentRender(nxui::Renderer& ren) override;
    nxui::Vec2 computeContentSize() const override;

private:
    bool m_bubbleVisible = true;
    nxui::Font* m_font = nullptr;
    float m_level   = -1.f;
    bool  m_charging = false;
    float m_timer    = 0.f;
    float m_chargeAnim = 0.f;
    nxui::Color m_textColor {1.f, 1.f, 1.f, 1.f};
};
