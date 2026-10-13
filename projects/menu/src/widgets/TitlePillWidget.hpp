#pragma once
#include <nxui/widgets/GlassWidget.hpp>
#include <nxui/core/Font.hpp>
#include <nxui/core/Animation.hpp>
#include <nxui/core/Types.hpp>
#include <string>


class TitlePillWidget : public nxui::GlassWidget {
public:
    TitlePillWidget();

    void setFont(nxui::Font* f)              { m_font = f; }
    void setTextColor(const nxui::Color& c)  { m_textColor = c; }

    void setText(const std::string& text, float screenWidth = 1280.f);
    void hideAnimated(float screenWidth = 1280.f);
    // Off draws only the name, with no glass bubble behind it.
    void setBubbleVisible(bool visible) { m_bubbleVisible = visible; }
    // Horizontal centre of the label. Negative = centre of the screen.
    void setAnchorCenterX(float x);

protected:
    void onRender(nxui::Renderer& ren) override;
    void onContentUpdate(float dt) override;
    void onContentRender(nxui::Renderer& ren) override;
    nxui::Vec2 computeContentSize() const override;

private:
    nxui::Font*       m_font = nullptr;
    std::string m_sourceText;
    std::string m_text;
    nxui::Color       m_textColor {1.f, 1.f, 1.f, 1.f};
    nxui::AnimatedFloat m_animX{0.f};
    nxui::AnimatedFloat m_animW{0.f};
    nxui::AnimatedFloat m_textReveal{1.f};
    bool m_layoutInitialized = false;
    bool m_hideOnCollapse = false;
    bool m_bubbleVisible = true;
    float m_anchorCenterX = -1.f;
    float centerFor(float screenWidth) const {
        return m_anchorCenterX >= 0.f ? m_anchorCenterX : screenWidth * 0.5f;
    }
};
