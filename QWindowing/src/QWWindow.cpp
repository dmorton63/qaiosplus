// QWindowing Window - Window class implementation
// Namespace: QW

#include "QWWindow.h"
#include "QWWindowManager.h"
#include "QKMemHeap.h"
#include "QCMemUtil.h"

namespace QW
{

    Window::Window(const char *title, Rect bounds)
        : m_windowId(0),
          m_bounds(bounds),
          m_flags(WindowFlags::Default),
          m_buffer(nullptr),
          m_bufferWidth(0),
          m_bufferHeight(0)
    {
        setTitle(title);

        // Allocate content buffer
        m_bufferWidth = bounds.width;
        m_bufferHeight = bounds.height;
        QC::usize bufferBytes = m_bufferWidth * m_bufferHeight * sizeof(QC::u32);
        m_buffer = static_cast<QC::u32 *>(QK::Memory::Heap::instance().allocate(bufferBytes));
        if (m_buffer)
        {
            memset(m_buffer, 0, bufferBytes);
        }
    }

    Window::~Window()
    {
        if (m_buffer)
        {
            QK::Memory::Heap::instance().free(m_buffer);
            m_buffer = nullptr;
        }
    }

    bool Window::onEvent(const QK::Event::Event &event)
    {
        using namespace QK::Event;

        // Only handle events targeted at this window or global input events
        if (event.isWindow())
        {
            const auto &winEvent = event.asWindow();
            if (winEvent.windowId != m_windowId)
            {
                return false;
            }

            switch (event.type())
            {
            case Type::WindowFocus:
                onFocus();
                return true;
            case Type::WindowBlur:
                onBlur();
                return true;
            case Type::WindowResize:
                onResize(winEvent.width, winEvent.height);
                return true;
            case Type::WindowPaint:
                onPaint();
                return true;
            case Type::WindowDestroy:
                onClose();
                return true;
            default:
                break;
            }
        }
        else if (event.isInput() && isFocused())
        {
            switch (event.type())
            {
            case Type::MouseMove:
            {
                const auto &mouse = event.asMouse();
                // Convert to window-local coordinates
                QC::i32 localX = mouse.x - m_bounds.x;
                QC::i32 localY = mouse.y - m_bounds.y;
                onMouseMove(localX, localY, mouse.deltaX, mouse.deltaY);
                return true;
            }
            case Type::MouseButtonDown:
            {
                const auto &mouse = event.asMouse();
                QC::i32 localX = mouse.x - m_bounds.x;
                QC::i32 localY = mouse.y - m_bounds.y;
                onMouseDown(localX, localY, mouse.button);
                return true;
            }
            case Type::MouseButtonUp:
            {
                const auto &mouse = event.asMouse();
                QC::i32 localX = mouse.x - m_bounds.x;
                QC::i32 localY = mouse.y - m_bounds.y;
                onMouseUp(localX, localY, mouse.button);
                return true;
            }
            case Type::MouseScroll:
            {
                const auto &mouse = event.asMouse();
                onMouseScroll(mouse.scrollDelta);
                return true;
            }
            case Type::KeyDown:
            {
                const auto &key = event.asKey();
                onKeyDown(key.scancode, key.keycode, key.character, key.modifiers);
                return true;
            }
            case Type::KeyUp:
            {
                const auto &key = event.asKey();
                onKeyUp(key.scancode, key.keycode, key.modifiers);
                return true;
            }
            default:
                break;
            }
        }

        return false;
    }

    QK::Event::Category Window::getEventMask() const
    {
        return QK::Event::Category::Input | QK::Event::Category::Window;
    }

    void Window::setTitle(const char *title)
    {
        if (title)
        {
            strncpy(m_title, title, sizeof(m_title) - 1);
            m_title[sizeof(m_title) - 1] = '\0';
        }
        else
        {
            m_title[0] = '\0';
        }
    }

    void Window::setBounds(const Rect &bounds)
    {
        bool sizeChanged = (bounds.width != m_bounds.width ||
                            bounds.height != m_bounds.height);
        m_bounds = bounds;

        if (sizeChanged)
        {
            // Reallocate buffer
            if (m_buffer)
            {
                QK::Memory::Heap::instance().free(m_buffer);
            }

            m_bufferWidth = bounds.width;
            m_bufferHeight = bounds.height;
            QC::usize bufferBytes = m_bufferWidth * m_bufferHeight * sizeof(QC::u32);
            m_buffer = static_cast<QC::u32 *>(QK::Memory::Heap::instance().allocate(bufferBytes));
            if (m_buffer)
            {
                memset(m_buffer, 0, bufferBytes);
            }
        }
    }

    Rect Window::clientRect() const
    {
        // TODO: Account for window decorations (title bar, borders)
        return Rect{0, 0, m_bounds.width, m_bounds.height};
    }

    void Window::setVisible(bool visible)
    {
        if (visible)
        {
            m_flags |= WindowFlags::Visible;
        }
        else
        {
            m_flags &= ~WindowFlags::Visible;
        }
    }

    bool Window::isFocused() const
    {
        return WindowManager::instance().focusedWindow() == this;
    }

    void Window::clear(Color color)
    {
        if (!m_buffer)
            return;

        QC::usize pixels = m_bufferWidth * m_bufferHeight;
        for (QC::usize i = 0; i < pixels; ++i)
        {
            m_buffer[i] = color.value;
        }
    }

    void Window::setPixel(QC::i32 x, QC::i32 y, Color color)
    {
        if (!m_buffer)
            return;
        if (x < 0 || y < 0)
            return;
        if (static_cast<QC::u32>(x) >= m_bufferWidth ||
            static_cast<QC::u32>(y) >= m_bufferHeight)
            return;

        m_buffer[y * m_bufferWidth + x] = color.value;
    }

    void Window::fillRect(const Rect &rect, Color color)
    {
        if (!m_buffer)
            return;

        QC::i32 x1 = rect.x < 0 ? 0 : rect.x;
        QC::i32 y1 = rect.y < 0 ? 0 : rect.y;
        QC::i32 x2 = rect.x + static_cast<QC::i32>(rect.width);
        QC::i32 y2 = rect.y + static_cast<QC::i32>(rect.height);

        if (x2 > static_cast<QC::i32>(m_bufferWidth))
            x2 = static_cast<QC::i32>(m_bufferWidth);
        if (y2 > static_cast<QC::i32>(m_bufferHeight))
            y2 = static_cast<QC::i32>(m_bufferHeight);

        for (QC::i32 y = y1; y < y2; ++y)
        {
            for (QC::i32 x = x1; x < x2; ++x)
            {
                m_buffer[y * m_bufferWidth + x] = color.value;
            }
        }
    }

    void Window::drawRect(const Rect &rect, Color color)
    {
        // Top
        fillRect(Rect{rect.x, rect.y, rect.width, 1}, color);
        // Bottom
        fillRect(Rect{rect.x, rect.y + static_cast<QC::i32>(rect.height) - 1, rect.width, 1}, color);
        // Left
        fillRect(Rect{rect.x, rect.y, 1, rect.height}, color);
        // Right
        fillRect(Rect{rect.x + static_cast<QC::i32>(rect.width) - 1, rect.y, 1, rect.height}, color);
    }

    void Window::drawText(QC::i32 x, QC::i32 y, const char *text, Color color)
    {
        (void)x;
        (void)y;
        (void)text;
        (void)color;
        // TODO: Implement text rendering with font system
    }

    void Window::invalidate()
    {
        WindowManager::instance().invalidate(m_bounds);
    }

    void Window::invalidateRect(const Rect &rect)
    {
        Rect screenRect{
            m_bounds.x + rect.x,
            m_bounds.y + rect.y,
            rect.width,
            rect.height};
        WindowManager::instance().invalidate(screenRect);
    }

    // Default event handlers - can be overridden
    void Window::onMouseMove(QC::i32 x, QC::i32 y, QC::i32 deltaX, QC::i32 deltaY)
    {
        (void)x;
        (void)y;
        (void)deltaX;
        (void)deltaY;
    }

    void Window::onMouseDown(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
    {
        (void)x;
        (void)y;
        (void)button;
    }

    void Window::onMouseUp(QC::i32 x, QC::i32 y, QK::Event::MouseButton button)
    {
        (void)x;
        (void)y;
        (void)button;
    }

    void Window::onMouseScroll(QC::i32 delta)
    {
        (void)delta;
    }

    void Window::onKeyDown(QC::u8 scancode, QC::u8 keycode, char character, QK::Event::Modifiers mods)
    {
        (void)scancode;
        (void)keycode;
        (void)character;
        (void)mods;
    }

    void Window::onKeyUp(QC::u8 scancode, QC::u8 keycode, QK::Event::Modifiers mods)
    {
        (void)scancode;
        (void)keycode;
        (void)mods;
    }

    void Window::onFocus()
    {
    }

    void Window::onBlur()
    {
    }

    void Window::onResize(QC::u32 width, QC::u32 height)
    {
        (void)width;
        (void)height;
    }

    void Window::onPaint()
    {
    }

    void Window::onClose()
    {
    }

} // namespace QW
