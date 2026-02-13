#include "QWControls/Base/ControlBase.h"
#include "QWControls/Containers/Panel.h"
#include "QWWindow.h"

namespace QW
{
    namespace Controls
    {

        ControlId ControlBase::s_nextId = 1;

        ControlBase::ControlBase()
            : m_id(s_nextId++),
              m_parent(nullptr),
              m_window(nullptr),
              m_bounds{0, 0, 0, 0},
              m_enabled(true),
              m_visible(true),
              m_focused(false),
              m_state(ControlState::Normal)
        {
        }

        ControlBase::ControlBase(Window *window, Rect bounds)
            : m_id(s_nextId++),
              m_parent(nullptr),
              m_window(window),
              m_bounds(bounds),
              m_enabled(true),
              m_visible(true),
              m_focused(false),
              m_state(ControlState::Normal)
        {
        }

        Rect ControlBase::absoluteBounds() const
        {
            Rect abs = m_bounds;
            Panel *current = m_parent;

            while (current)
            {
                Rect parentBounds = current->bounds();
                abs.x += parentBounds.x;
                abs.y += parentBounds.y;
                current = current->parent();
            }

            return abs;
        }

        bool ControlBase::hitTest(int x, int y) const
        {
            Rect abs = absoluteBounds();
            return x >= abs.x && x < abs.x + abs.width &&
                   y >= abs.y && y < abs.y + abs.height;
        }

        void ControlBase::setEnabled(bool enabled)
        {
            m_enabled = enabled;
            m_state = enabled ? ControlState::Normal : ControlState::Disabled;
        }

        void ControlBase::setFocused(bool focused)
        {
            if (m_focused == focused)
                return;

            m_focused = focused;
            if (focused)
                onFocus();
            else
                onBlur();
        }

        void ControlBase::invalidate()
        {
            if (m_window)
                m_window->invalidateRect(absoluteBounds());
        }

        bool ControlBase::onEvent(const QK::Event::Event &event)
        {
            if (!m_enabled || !m_visible)
                return false;

            switch (event.type())
            {
            case QK::Event::Type::MouseMove:
                return onMouseMove(event.asMouse().x, event.asMouse().y,
                                   event.asMouse().deltaX, event.asMouse().deltaY);

            case QK::Event::Type::MouseButtonDown:
                return onMouseDown(event.asMouse().x, event.asMouse().y,
                                   event.asMouse().button);

            case QK::Event::Type::MouseButtonUp:
                return onMouseUp(event.asMouse().x, event.asMouse().y,
                                 event.asMouse().button);

            case QK::Event::Type::MouseScroll:
                return onMouseScroll(event.asMouse().scrollDelta);

            case QK::Event::Type::KeyDown:
                return onKeyDown(event.asKey().scancode, event.asKey().keycode,
                                 event.asKey().character, event.asKey().modifiers);

            case QK::Event::Type::KeyUp:
                return onKeyUp(event.asKey().scancode, event.asKey().keycode,
                               event.asKey().modifiers);

            case QK::Event::Type::WindowFocus:
                onFocus();
                return true;

            case QK::Event::Type::WindowBlur:
                onBlur();
                return true;

            default:
                return false;
            }
        }

    } // namespace Controls
} // namespace QW