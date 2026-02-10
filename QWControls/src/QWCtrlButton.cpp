// QWControls Button - Button control implementation
// Namespace: QW::Controls

#include "QWCtrlButton.h"
#include "QCMemUtil.h"
#include "QCString.h"

namespace QW
{
    namespace Controls
    {

        Button::Button(Window *parent, const char *text, Rect bounds)
            : m_parent(parent),
              m_bounds(bounds),
              m_enabled(true),
              m_state(ButtonState::Normal),
              m_bgColor(Color(200, 200, 200, 255)),
              m_textColor(Color(0, 0, 0, 255)),
              m_borderColor(Color(100, 100, 100, 255)),
              m_clickHandler(nullptr),
              m_clickUserData(nullptr)
        {
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

        void Button::setEnabled(bool enabled)
        {
            m_enabled = enabled;
            m_state = enabled ? ButtonState::Normal : ButtonState::Disabled;
        }

        void Button::setClickHandler(ButtonClickHandler handler, void *userData)
        {
            m_clickHandler = handler;
            m_clickUserData = userData;
        }

        void Button::paint()
        {
            if (!m_parent)
                return;

            // Draw background based on state
            Color bgColor = m_bgColor;
            switch (m_state)
            {
            case ButtonState::Hovered:
                bgColor = Color(220, 220, 220, 255);
                break;
            case ButtonState::Pressed:
                bgColor = Color(180, 180, 180, 255);
                break;
            case ButtonState::Disabled:
                bgColor = Color(160, 160, 160, 255);
                break;
            default:
                break;
            }

            // TODO: Use window drawing primitives
            // m_parent->fillRect(m_bounds, bgColor);
            // m_parent->drawRect(m_bounds, m_borderColor);
            // m_parent->drawText(m_text, m_bounds, m_textColor, TextAlign::Center);
        }

        void Button::handleMouseMove(QC::i32 x, QC::i32 y)
        {
            if (!m_enabled)
                return;

            bool inside = x >= m_bounds.x && x < m_bounds.x + static_cast<QC::i32>(m_bounds.width) &&
                          y >= m_bounds.y && y < m_bounds.y + static_cast<QC::i32>(m_bounds.height);

            if (inside && m_state == ButtonState::Normal)
            {
                m_state = ButtonState::Hovered;
            }
            else if (!inside && m_state == ButtonState::Hovered)
            {
                m_state = ButtonState::Normal;
            }
        }

        void Button::handleMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
        {
            if (!m_enabled || button != QK::Event::MouseButton::Left)
                return;

            bool inside = x >= m_bounds.x && x < m_bounds.x + static_cast<QC::i32>(m_bounds.width) &&
                          y >= m_bounds.y && y < m_bounds.y + static_cast<QC::i32>(m_bounds.height);

            if (inside)
            {
                m_state = ButtonState::Pressed;
            }
        }

        void Button::handleMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
        {
            if (!m_enabled || button != QK::Event::MouseButton::Left)
                return;

            bool inside = x >= m_bounds.x && x < m_bounds.x + static_cast<QC::i32>(m_bounds.width) &&
                          y >= m_bounds.y && y < m_bounds.y + static_cast<QC::i32>(m_bounds.height);

            if (m_state == ButtonState::Pressed)
            {
                m_state = inside ? ButtonState::Hovered : ButtonState::Normal;

                if (inside && m_clickHandler)
                {
                    m_clickHandler(this, m_clickUserData);
                }
            }
        }

    } // namespace Controls
} // namespace QW
