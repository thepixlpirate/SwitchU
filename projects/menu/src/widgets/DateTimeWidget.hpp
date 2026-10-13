#pragma once
#include <nxui/widgets/GlassWidget.hpp>
#include <nxui/core/Font.hpp>
#include <nxui/core/Types.hpp>
#include <string>

namespace switchu::services {
class ClockService;
}

class DateTimeWidget : public nxui::GlassWidget {
public:
    DateTimeWidget() = default;
    void setFont(nxui::Font* f) { m_font = f; }
    void setSmallFont(nxui::Font* sf) { m_smallFont = sf; }
    void setTextColor(const nxui::Color& c) { m_textColor = c; }
    void setSecondaryTextColor(const nxui::Color& c) { m_secondaryColor = c; }
    void setClockService(switchu::services::ClockService* service) { m_clockService = service; }
    void setUse12HourClock(bool enabled);
    void setShowDate(bool show) { m_showDate = show; }

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
    bool m_showDate = true;
    nxui::Font* m_font = nullptr;
    nxui::Font* m_smallFont = nullptr;
    float m_timer = 0.f;
    std::string m_timeStr;
    std::string m_dateStr;
    bool m_use12HourClock = false;
    switchu::services::ClockService* m_clockService = nullptr;
    nxui::Color m_textColor      {1.f, 1.f, 1.f, 1.f};
    nxui::Color m_secondaryColor {0.7f, 0.7f, 0.8f, 0.8f};
};
