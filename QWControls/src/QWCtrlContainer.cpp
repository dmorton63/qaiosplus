// QWControls Container - Pure container control implementation
// Namespace: QW::Controls

#include "QWCtrlContainer.h"
#include "QWCtrlPanel.h"
#include "QWWindow.h"

namespace QW
{
    namespace Controls
    {

        Container::Container()
            : ControlBase(),
              m_focusedChild(nullptr),
              m_hoveredChild(nullptr),
              m_capturedChild(nullptr)
        {
        }

        Container::Container(Window *window, Rect bounds)
            : ControlBase(window, bounds),
              m_focusedChild(nullptr),
              m_hoveredChild(nullptr),
              m_capturedChild(nullptr)
        {
        }

        Container::~Container()
        {
            // Note: We don't own the children, just clear the vector
            m_children.clear();
        }

        // ==================== Type Information ====================

        Panel *Container::asPanel()
        {
            // Container is not a Panel - subclasses override if they are
            return nullptr;
        }

        const Panel *Container::asPanel() const
        {
            return nullptr;
        }

        // ==================== Child Management ====================

        void Container::addChild(IControl *child)
        {
            if (!child)
                return;

            // Remove from previous parent if any
            Panel *oldParent = child->parent();
            if (oldParent && oldParent != this->asPanel())
            {
                oldParent->removeChild(child);
            }

            child->setParent(this->asPanel());
            child->setWindow(m_window);
            m_children.push_back(child);
        }

        void Container::removeChild(IControl *child)
        {
            if (!child)
                return;

            for (QC::usize i = 0; i < m_children.size(); ++i)
            {
                if (m_children[i] == child)
                {
                    // Clear focus/hover if this was the focused/hovered child
                    if (m_focusedChild == child)
                        m_focusedChild = nullptr;
                    if (m_hoveredChild == child)
                        m_hoveredChild = nullptr;
                    if (m_capturedChild == child)
                        m_capturedChild = nullptr;

                    child->setParent(nullptr);

                    // Shift remaining elements down
                    for (QC::usize j = i; j < m_children.size() - 1; ++j)
                    {
                        m_children[j] = m_children[j + 1];
                    }
                    m_children.pop_back();
                    return;
                }
            }
        }

        void Container::removeChildAt(QC::usize index)
        {
            if (index >= m_children.size())
                return;

            IControl *child = m_children[index];
            removeChild(child);
        }

        void Container::clearChildren()
        {
            for (QC::usize i = 0; i < m_children.size(); ++i)
            {
                m_children[i]->setParent(nullptr);
            }
            m_children.clear();
            m_focusedChild = nullptr;
            m_hoveredChild = nullptr;
            m_capturedChild = nullptr;
        }

        IControl *Container::childAt(QC::usize index)
        {
            if (index >= m_children.size())
                return nullptr;
            return m_children[index];
        }

        const IControl *Container::childAt(QC::usize index) const
        {
            if (index >= m_children.size())
                return nullptr;
            return m_children[index];
        }

        IControl *Container::findChild(ControlId id)
        {
            for (QC::usize i = 0; i < m_children.size(); ++i)
            {
                if (m_children[i]->id() == id)
                    return m_children[i];

                // Recursively search in child containers
                if (m_children[i]->isContainer())
                {
                    Panel *childPanel = m_children[i]->asPanel();
                    if (childPanel)
                    {
                        IControl *found = childPanel->findChild(id);
                        if (found)
                            return found;
                    }
                }
            }
            return nullptr;
        }

        IControl *Container::childAtPoint(QC::i32 x, QC::i32 y)
        {
            // Search in reverse order (topmost first)
            for (QC::isize i = static_cast<QC::isize>(m_children.size()) - 1; i >= 0; --i)
            {
                IControl *child = m_children[static_cast<QC::usize>(i)];
                if (child->isVisible() && child->hitTest(x, y))
                {
                    return child;
                }
            }
            return nullptr;
        }

        // ==================== Rendering ====================

        void Container::paint()
        {
            if (!m_visible || !m_window)
                return;

            // Container has no visual representation - just paint children
            paintChildren();
        }

        void Container::paintChildren()
        {
            for (QC::usize i = 0; i < m_children.size(); ++i)
            {
                if (m_children[i]->isVisible())
                {
                    m_children[i]->paint();
                }
            }
        }

        // ==================== Event Handling ====================

        bool Container::onEvent(const QK::Event::Event &event)
        {
            if (!m_enabled || !m_visible)
                return false;

            // Route to base class which dispatches to specific handlers
            return ControlBase::onEvent(event);
        }

        bool Container::onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY)
        {
            // If we have a captured child (mouse button held), send to it
            if (m_capturedChild)
            {
                return m_capturedChild->onMouseMove(x, y, deltaX, deltaY);
            }

            // Find child under mouse
            IControl *child = childAtPoint(x, y);

            // Handle hover transitions
            if (child != m_hoveredChild)
            {
                // Exit from old hovered child
                if (m_hoveredChild)
                {
                    m_hoveredChild->onMouseMove(x, y, deltaX, deltaY);
                }

                m_hoveredChild = child;
            }

            // Send move to current hovered child
            if (child)
            {
                return child->onMouseMove(x, y, deltaX, deltaY);
            }

            return false;
        }

        bool Container::onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
        {
            IControl *child = childAtPoint(x, y);

            if (child)
            {
                // Capture mouse to this child
                m_capturedChild = child;

                // Set focus
                setFocusedChild(child);

                return child->onMouseDown(x, y, button);
            }

            return false;
        }

        bool Container::onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
        {
            bool handled = false;

            if (m_capturedChild)
            {
                handled = m_capturedChild->onMouseUp(x, y, button);
                m_capturedChild = nullptr;
            }
            else
            {
                IControl *child = childAtPoint(x, y);
                if (child)
                {
                    handled = child->onMouseUp(x, y, button);
                }
            }

            return handled;
        }

        bool Container::onMouseScroll(QC::i32 delta)
        {
            // Send scroll to hovered or focused child
            if (m_hoveredChild)
            {
                return m_hoveredChild->onMouseScroll(delta);
            }
            if (m_focusedChild)
            {
                return m_focusedChild->onMouseScroll(delta);
            }
            return false;
        }

        bool Container::onKeyDown(QC::u8 scancode, QC::u8 keycode, char character, QK::Event::Modifiers mods)
        {
            // Send keyboard events to focused child
            if (m_focusedChild)
            {
                return m_focusedChild->onKeyDown(scancode, keycode, character, mods);
            }
            return false;
        }

        bool Container::onKeyUp(QC::u8 scancode, QC::u8 keycode, QK::Event::Modifiers mods)
        {
            if (m_focusedChild)
            {
                return m_focusedChild->onKeyUp(scancode, keycode, mods);
            }
            return false;
        }

        // ==================== Focus Management ====================

        void Container::setFocusedChild(IControl *child)
        {
            if (m_focusedChild == child)
                return;

            // Blur old focused child
            if (m_focusedChild)
            {
                m_focusedChild->setFocused(false);
            }

            m_focusedChild = child;

            // Focus new child
            if (m_focusedChild)
            {
                m_focusedChild->setFocused(true);
            }
        }

        void Container::focusNext()
        {
            if (m_children.empty())
                return;

            QC::usize startIndex = 0;

            // Find current focused index
            if (m_focusedChild)
            {
                for (QC::usize i = 0; i < m_children.size(); ++i)
                {
                    if (m_children[i] == m_focusedChild)
                    {
                        startIndex = i + 1;
                        break;
                    }
                }
            }

            // Find next enabled, visible control
            for (QC::usize i = 0; i < m_children.size(); ++i)
            {
                QC::usize index = (startIndex + i) % m_children.size();
                IControl *child = m_children[index];
                if (child->isEnabled() && child->isVisible())
                {
                    setFocusedChild(child);
                    return;
                }
            }
        }

        void Container::focusPrevious()
        {
            if (m_children.empty())
                return;

            QC::isize startIndex = static_cast<QC::isize>(m_children.size()) - 1;

            // Find current focused index
            if (m_focusedChild)
            {
                for (QC::usize i = 0; i < m_children.size(); ++i)
                {
                    if (m_children[i] == m_focusedChild)
                    {
                        startIndex = static_cast<QC::isize>(i) - 1;
                        break;
                    }
                }
            }

            // Find previous enabled, visible control
            for (QC::usize i = 0; i < m_children.size(); ++i)
            {
                QC::isize index = startIndex - static_cast<QC::isize>(i);
                if (index < 0)
                    index += static_cast<QC::isize>(m_children.size());
                IControl *child = m_children[static_cast<QC::usize>(index)];
                if (child->isEnabled() && child->isVisible())
                {
                    setFocusedChild(child);
                    return;
                }
            }
        }

        // ==================== Coordinate Conversion ====================

        Point Container::windowToLocal(QC::i32 x, QC::i32 y) const
        {
            Rect abs = absoluteBounds();
            return {x - abs.x, y - abs.y};
        }

        Point Container::localToWindow(QC::i32 x, QC::i32 y) const
        {
            Rect abs = absoluteBounds();
            return {x + abs.x, y + abs.y};
        }

    } // namespace Controls
} // namespace QW
