// Default control implementation
// Namespace: QW::Controls

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
              m_state(ControlState::Normal),
              m_bgColor(QC::Color(240, 240, 240, 255))
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
              m_state(ControlState::Normal),
              m_bgColor(QC::Color(240, 240, 240, 255))
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

        bool ControlBase::hitTest(QC::i32 x, QC::i32 y) const
        {
            QC::Rect abs = absoluteBounds();
            return x >= abs.x && x < abs.x + static_cast<QC::i32>(abs.width) &&
                   y >= abs.y && y < abs.y + static_cast<QC::i32>(abs.height);
        }

        void ControlBase::setEnabled(bool enabled)
        {
            m_enabled = enabled;
            m_state = enabled ? ControlState::Normal : ControlState::Disabled;
        }

        void ControlBase::setFocused(bool focused)
        {
            if (m_focused == focused)
            {
                return;
            }

            m_focused = focused;
            if (focused)
            {
                onFocus();
            }
            else
            {
                onBlur();
            }
        }

        void ControlBase::invalidate()
        {
            if (m_window)
            {
                m_window->invalidateRect(absoluteBounds());
            }
        }

        bool ControlBase::onEvent(const QK::Event::Event &event)
        {
            if (!m_enabled || !m_visible)
            {
                return false;
            }

            switch (event.type())
            {
            case QK::Event::Type::MouseMove:
            {
                const auto &mouse = event.asMouse();
                return onMouseMove(mouse.x, mouse.y, mouse.deltaX, mouse.deltaY);
            }
            case QK::Event::Type::MouseButtonDown:
            {
                const auto &mouse = event.asMouse();
                return onMouseDown(mouse.x, mouse.y, mouse.button);
            }
            case QK::Event::Type::MouseButtonUp:
            {
                const auto &mouse = event.asMouse();
                return onMouseUp(mouse.x, mouse.y, mouse.button);
            }
            case QK::Event::Type::MouseScroll:
            {
                const auto &mouse = event.asMouse();
                return onMouseScroll(mouse.scrollDelta);
            }
            case QK::Event::Type::KeyDown:
            {
                const auto &key = event.asKey();
                return onKeyDown(key.scancode, key.keycode, key.character, key.modifiers);
            }
            case QK::Event::Type::KeyUp:
            {
                const auto &key = event.asKey();
                return onKeyUp(key.scancode, key.keycode, key.modifiers);
            }
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
    }
} // namespace QW
