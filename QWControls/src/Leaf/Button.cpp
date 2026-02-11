// QWControls Button - Button control implementation
// Namespace: QW::Controls

#include "QWControls/Leaf/Button.h"
#include "QCMemUtil.h"
#include "QCString.h"
#include "QWWindow.h"

namespace
{
    inline QC::i32 clampChannel(QC::i32 value)
    {
        if (value < 0)
            return 0;
        if (value > 255)
            return 255;
        return value;
    }

    inline QW::Color blendTowards(QW::Color color, QC::u8 target, QC::u8 percent)
    {
        if (percent > 100)
            percent = 100;

        auto adjust = [&](QC::u8 component) -> QC::u8
        {
            QC::i32 delta = static_cast<QC::i32>(target) - static_cast<QC::i32>(component);
            QC::i32 value = static_cast<QC::i32>(component) + (delta * percent + 50) / 100;
            return static_cast<QC::u8>(clampChannel(value));
        };

        return QW::Color(adjust(color.r), adjust(color.g), adjust(color.b), color.a);
    }

    inline QW::Color lightenColor(QW::Color color, QC::u8 percent)
    {
        return blendTowards(color, 255, percent);
    }

    inline QW::Color darkenColor(QW::Color color, QC::u8 percent)
    {
        return blendTowards(color, 0, percent);
    }
} // namespace

namespace QW
{
    namespace Controls
    {

        Button::Button()
            : ControlBase(),
              m_textColor(Color(0, 0, 0, 255)),
              m_borderColor(Color(100, 100, 100, 255)),
              m_hoverColor(Color(220, 220, 220, 255)),
              m_pressedColor(Color(180, 180, 180, 255)),
              m_visualStyle(ButtonStyle::Flat),
              m_vistaGradientTop(Color(140, 196, 255, 255)),
              m_vistaGradientBottom(Color(30, 90, 170, 255)),
              m_vistaHighlightTop(Color(255, 255, 255, 200)),
              m_vistaHighlightBottom(Color(255, 255, 255, 30)),
              m_vistaGlow(Color(140, 196, 255, 140)),
              m_vistaBorderOuter(Color(15, 40, 80, 255)),
              m_vistaBorderInner(Color(255, 255, 255, 140)),
              m_clickHandler(nullptr),
              m_clickUserData(nullptr)
        {
            m_bgColor = Color(200, 200, 200, 255);
            m_text[0] = '\0';
        }

        Button::Button(Window *window, const char *text, Rect bounds)
            : ControlBase(window, bounds),
              m_textColor(Color(0, 0, 0, 255)),
              m_borderColor(Color(100, 100, 100, 255)),
              m_hoverColor(Color(220, 220, 220, 255)),
              m_pressedColor(Color(180, 180, 180, 255)),
              m_visualStyle(ButtonStyle::Flat),
              m_vistaGradientTop(Color(140, 196, 255, 255)),
              m_vistaGradientBottom(Color(30, 90, 170, 255)),
              m_vistaHighlightTop(Color(255, 255, 255, 200)),
              m_vistaHighlightBottom(Color(255, 255, 255, 30)),
              m_vistaGlow(Color(140, 196, 255, 140)),
              m_vistaBorderOuter(Color(15, 40, 80, 255)),
              m_vistaBorderInner(Color(255, 255, 255, 140)),
              m_clickHandler(nullptr),
              m_clickUserData(nullptr)
        {
            m_bgColor = Color(200, 200, 200, 255);
            setText(text);
        }

        Button::~Button()
        {
        }

        void Button::setText(const char *text)
        {
            if (text)
            {
                strncpy(m_text, text, sizeof(m_text) - 1);
                m_text[sizeof(m_text) - 1] = '\0';
            }
            else
            {
                m_text[0] = '\0';
            }
        }

        void Button::setClickHandler(ButtonClickHandler handler, void *userData)
        {
            m_clickHandler = handler;
            m_clickUserData = userData;
        }

        void Button::setVisualStyle(ButtonStyle style)
        {
            if (m_visualStyle == style)
                return;

            m_visualStyle = style;
            invalidate();
        }

        void Button::setVistaGradient(Color top, Color bottom)
        {
            m_vistaGradientTop = top;
            m_vistaGradientBottom = bottom;
            if (m_visualStyle == ButtonStyle::Vista)
                invalidate();
        }

        void Button::setVistaHighlight(Color top, Color bottom)
        {
            m_vistaHighlightTop = top;
            m_vistaHighlightBottom = bottom;
            if (m_visualStyle == ButtonStyle::Vista)
                invalidate();
        }

        void Button::setVistaBorders(Color outer, Color inner)
        {
            m_vistaBorderOuter = outer;
            m_vistaBorderInner = inner;
            if (m_visualStyle == ButtonStyle::Vista)
                invalidate();
        }

        void Button::setVistaGlow(Color glow)
        {
            m_vistaGlow = glow;
            if (m_visualStyle == ButtonStyle::Vista)
                invalidate();
        }

        void Button::paint()
        {
            if (!m_window || !m_visible)
                return;

            Rect abs = absoluteBounds();

            if (m_visualStyle == ButtonStyle::Vista)
            {
                paintVista(abs);
                return;
            }

            // Draw background based on state
            Color bgColor = m_bgColor;
            switch (m_state)
            {
            case ControlState::Hovered:
                bgColor = m_hoverColor;
                break;
            case ControlState::Pressed:
                bgColor = m_pressedColor;
                break;
            case ControlState::Disabled:
                bgColor = Color(160, 160, 160, 255);
                break;
            default:
                break;
            }

            m_window->fillRect(abs, bgColor);
            m_window->drawRect(abs, m_borderColor);

            // Draw centered text
            QC::Size textSize = m_window->measureText(m_text);
            QC::i32 textX = abs.x + (static_cast<QC::i32>(abs.width) - static_cast<QC::i32>(textSize.width)) / 2;
            QC::i32 textY = abs.y + (static_cast<QC::i32>(abs.height) - static_cast<QC::i32>(textSize.height)) / 2;
            m_window->drawText(textX, textY, m_text, m_textColor);
        }

        void Button::paintVista(const Rect &abs)
        {
            Color top = m_vistaGradientTop;
            Color bottom = m_vistaGradientBottom;
            Color highlightTop = m_vistaHighlightTop;
            Color highlightBottom = m_vistaHighlightBottom;
            Color glow = m_vistaGlow;

            switch (m_state)
            {
            case ControlState::Hovered:
            {
                top = lightenColor(top, 15);
                bottom = lightenColor(bottom, 15);
                QC::u32 boostedAlpha = static_cast<QC::u32>(glow.a) + 40;
                if (boostedAlpha > 255)
                    boostedAlpha = 255;
                Color glowBoosted = lightenColor(glow, 10);
                glow = glowBoosted.withAlpha(static_cast<QC::u8>(boostedAlpha));
                break;
            }
            case ControlState::Pressed:
                top = darkenColor(bottom, 20);
                bottom = darkenColor(m_vistaGradientTop, 20);
                highlightTop = darkenColor(highlightTop, 40);
                highlightBottom = darkenColor(highlightBottom, 40);
                glow = darkenColor(glow, 40);
                break;
            case ControlState::Disabled:
                top = darkenColor(top, 40);
                bottom = darkenColor(bottom, 40);
                glow = glow.withAlpha(static_cast<QC::u8>(glow.a / 2));
                break;
            default:
                break;
            }

            m_window->fillGradientV(abs, top, bottom);

            Rect highlightRect = abs;
            QC::i32 halfHeight = static_cast<QC::i32>(highlightRect.height) / 2;
            if (halfHeight < 2)
                halfHeight = 2;
            highlightRect.height = halfHeight;
            m_window->fillGradientV(highlightRect, highlightTop, highlightBottom);

            Rect glowRect = abs;
            glowRect.y += static_cast<QC::i32>(glowRect.height) / 2;
            QC::i32 glowHeight = static_cast<QC::i32>(glowRect.height) / 2;
            if (glowHeight < 2)
                glowHeight = 2;
            glowRect.height = glowHeight;
            m_window->fillGradientV(glowRect, glow.withAlpha(glow.a), glow.withAlpha(0));

            m_window->drawRect(abs, m_vistaBorderOuter);
            if (abs.width > 2 && abs.height > 2)
            {
                Rect inner = {
                    abs.x + 1,
                    abs.y + 1,
                    static_cast<QC::i32>(abs.width) - 2,
                    static_cast<QC::i32>(abs.height) - 2};
                m_window->drawRect(inner, m_vistaBorderInner);
            }

            QC::Size textSize = m_window->measureText(m_text);
            QC::i32 textX = abs.x + (static_cast<QC::i32>(abs.width) - static_cast<QC::i32>(textSize.width)) / 2;
            QC::i32 textY = abs.y + (static_cast<QC::i32>(abs.height) - static_cast<QC::i32>(textSize.height)) / 2;

            Color shadowColor(0, 0, 0, 160);
            m_window->drawText(textX + 1, textY + 1, m_text, shadowColor);
            m_window->drawText(textX, textY, m_text, m_textColor);
        }

        bool Button::onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY)
        {
            if (!m_enabled)
                return false;

            bool inside = hitTest(x, y);

            if (inside && m_state == ControlState::Normal)
            {
                setState(ControlState::Hovered);
                invalidate();
                return true;
            }
            else if (!inside && m_state == ControlState::Hovered)
            {
                setState(ControlState::Normal);
                invalidate();
                return true;
            }

            return inside;
        }

        bool Button::onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
        {
            if (!m_enabled || button != QK::Event::MouseButton::Left)
                return false;

            bool inside = hitTest(x, y);

            if (inside)
            {
                setState(ControlState::Pressed);
                invalidate();
                return true;
            }

            return false;
        }

        bool Button::onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
        {
            if (!m_enabled || button != QK::Event::MouseButton::Left)
                return false;

            bool inside = hitTest(x, y);

            if (m_state == ControlState::Pressed)
            {
                setState(inside ? ControlState::Hovered : ControlState::Normal);
                invalidate();

                if (inside && m_clickHandler)
                {
                    m_clickHandler(this, m_clickUserData);
                }
                return true;
            }

            return false;
        }

    } // namespace Controls
} // namespace QW
