// QWControls Button - Button control implementation
// Namespace: QW::Controls

#include "QWControls/Leaf/Button.h"
#include "QWWindow.h"
#include <cstring>

namespace QW
{
    namespace Controls
    {

        Button::Button()
            : ControlBase()
        {
            m_text[0] = '\0';
        }

        Button::Button(Window *window, const char *text, Rect bounds)
            : ControlBase(window, bounds)
        {
            setText(text);
        }

        void Button::setText(const char *text)
        {
            if (text)
            {
                std::strncpy(m_text, text, sizeof(m_text) - 1);
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

        void Button::setRole(ButtonRole role)
        {
            if (m_role == role)
                return;

            m_role = role;
            invalidate();
        }

        void Button::paint(const PaintContext &ctx)
        {
            if (!ctx.styleRenderer || !m_window || !m_visible)
                return;

            ButtonPaintArgs args{};
            args.bounds = absoluteBounds();
            args.text = m_text;
            args.role = m_role;
            args.defaultButton = m_focused;

            // Map internal state to style state
            if (!m_enabled)
                args.state = ButtonPaintArgs::State::Disabled;
            else if (m_pressed)
                args.state = ButtonPaintArgs::State::Pressed;
            else if (m_hovered)
                args.state = ButtonPaintArgs::State::Hovered;
            else
                args.state = ButtonPaintArgs::State::Normal;

            ctx.styleRenderer->drawButton(args);
        }

        bool Button::onMouseMove(int x, int y, int dx, int dy)
        {
            if (!m_enabled)
                return false;

            bool inside = hitTest(x, y);

            if (inside && !m_hovered)
            {
                m_hovered = true;
                invalidate();
                return true;
            }
            if (!inside && m_hovered)
            {
                m_hovered = false;
                invalidate();
                return true;
            }

            return inside;
        }

        bool Button::onMouseDown(int x, int y, QK::Event::MouseButton button)
        {
            if (!m_enabled || button != QK::Event::MouseButton::Left)
                return false;

            if (hitTest(x, y))
            {
                m_pressed = true;
                invalidate();
                return true;
            }

            return false;
        }

        bool Button::onMouseUp(int x, int y, QK::Event::MouseButton button)
        {
            if (!m_enabled || button != QK::Event::MouseButton::Left)
                return false;

            bool inside = hitTest(x, y);

            if (m_pressed)
            {
                m_pressed = false;
                invalidate();

                if (inside && m_clickHandler)
                    m_clickHandler(this, m_clickUserData);

                return true;
            }

            return false;
        }

    } // namespace Controls
} // namespace QW