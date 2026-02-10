// QWControls Button - Button control implementation
// Namespace: QW::Controls

#include "QWCtrlButton.h"
#include "QCMemUtil.h"
#include "QCString.h"
#include "QWWindow.h"

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

        void Button::paint()
        {
            if (!m_window || !m_visible)
                return;

            Rect abs = absoluteBounds();

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
            QC::i32 textX = abs.x + static_cast<QC::i32>(abs.width / 2);
            QC::i32 textY = abs.y + static_cast<QC::i32>(abs.height / 2);
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
