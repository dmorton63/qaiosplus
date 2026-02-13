// QWindowing Window Manager - Desktop window management implementation
// Namespace: QW

#include "QWWindowManager.h"
#include "QWWindow.h"
#include "QWFramebuffer.h"
#include "QWCompositor.h"
#include "QWStyleSystem.h"
#include "QKEventManager.h"
#include "QKMemHeap.h"
#include "QCMemUtil.h"

namespace QW
{

    WindowManager &WindowManager::instance()
    {
        static WindowManager s_instance;
        return s_instance;
    }

    WindowManager::WindowManager()
        : m_nextWindowId(1),
          m_focusedWindow(nullptr),
          m_hoveredWindow(nullptr),
          m_framebuffer(nullptr),
          m_compositor(nullptr),
          m_mousePos{0, 0},
          m_listenerId(QK::Event::InvalidListenerId)
    {
    }

    WindowManager::~WindowManager()
    {
        shutdown();
    }

    void WindowManager::initialize(Framebuffer *fb)
    {
        m_framebuffer = fb;

        // Create compositor
        m_compositor = new Compositor(fb);
        m_compositor->initialize();

        auto &styleSystem = StyleSystem::instance();
        styleSystem.initialize();
        styleSystem.addListener(this);

        // Register as event listener for input events
        auto &eventMgr = QK::Event::EventManager::instance();
        QK::Event::EventListener listener;
        listener.categoryMask = QK::Event::Category::Input;
        listener.handler = [](const QK::Event::Event &event, void *userData) -> bool
        {
            auto *wm = static_cast<WindowManager *>(userData);
            return wm->onEvent(event);
        };
        listener.userData = this;

        m_listenerId = eventMgr.addListener(listener);
    }

    void WindowManager::shutdown()
    {
        StyleSystem::instance().removeListener(this);

        // Unregister from event manager
        if (m_listenerId != QK::Event::InvalidListenerId)
        {
            QK::Event::EventManager::instance().removeListener(m_listenerId);
            m_listenerId = QK::Event::InvalidListenerId;
        }

        // Destroy all windows
        for (QC::usize i = 0; i < m_windows.size(); ++i)
        {
            delete m_windows[i];
        }
        m_windows.clear();

        // Destroy compositor
        if (m_compositor)
        {
            delete m_compositor;
            m_compositor = nullptr;
        }

        m_focusedWindow = nullptr;
        m_hoveredWindow = nullptr;
    }

    bool WindowManager::onEvent(const QK::Event::Event &event)
    {
        using namespace QK::Event;

        if (!event.isInput())
        {
            return false;
        }

        switch (event.type())
        {
        case Type::MouseMove:
        case Type::MouseButtonDown:
        case Type::MouseButtonUp:
        case Type::MouseScroll:
            routeMouseEvent(event.asMouse());
            // Don't consume - let other listeners see the event
            return false;

        case Type::KeyDown:
        case Type::KeyUp:
        case Type::KeyPress:
            routeKeyEvent(event.asKey());
            // Don't consume - let other listeners see the event
            return false;

        default:
            break;
        }

        return false;
    }

    QK::Event::Category WindowManager::getEventMask() const
    {
        return QK::Event::Category::Input;
    }

    Window *WindowManager::createWindow(const char *title, Rect bounds)
    {
        Window *window = new Window(title, bounds);
        window->setWindowId(m_nextWindowId++);
        m_windows.push_back(window);

        applyStyleToWindow(window, StyleSystem::instance().currentStyle());

        // Post window create event
        postWindowEvent(QK::Event::Type::WindowCreate, window);

        return window;
    }

    void WindowManager::destroyWindow(Window *window)
    {
        if (!window)
            return;

        // Post window destroy event
        postWindowEvent(QK::Event::Type::WindowDestroy, window);

        // Clear focus if this window was focused
        if (m_focusedWindow == window)
        {
            m_focusedWindow = nullptr;
        }
        if (m_hoveredWindow == window)
        {
            m_hoveredWindow = nullptr;
        }

        // Remove from list
        for (QC::usize i = 0; i < m_windows.size(); ++i)
        {
            if (m_windows[i] == window)
            {
                // Shift elements left to fill the gap
                for (QC::usize j = i; j < m_windows.size() - 1; ++j)
                {
                    m_windows[j] = m_windows[j + 1];
                }
                m_windows.pop_back();
                break;
            }
        }

        delete window;
    }

    Window *WindowManager::windowById(QC::u32 id) const
    {
        for (QC::usize i = 0; i < m_windows.size(); ++i)
        {
            if (m_windows[i]->windowId() == id)
            {
                return m_windows[i];
            }
        }
        return nullptr;
    }

    void WindowManager::setFocus(Window *window)
    {
        if (m_focusedWindow == window)
            return;

        Window *oldFocus = m_focusedWindow;
        m_focusedWindow = window;

        // Post blur event to old window
        if (oldFocus)
        {
            postWindowEvent(QK::Event::Type::WindowBlur, oldFocus);
        }

        // Post focus event to new window
        if (window)
        {
            postWindowEvent(QK::Event::Type::WindowFocus, window);
        }
    }

    void WindowManager::bringToFront(Window *window)
    {
        if (!window)
            return;

        for (QC::usize i = 0; i < m_windows.size(); ++i)
        {
            if (m_windows[i] == window)
            {
                // Move to end (top of z-order)
                // Shift elements left to fill the gap
                for (QC::usize j = i; j < m_windows.size() - 1; ++j)
                {
                    m_windows[j] = m_windows[j + 1];
                }
                m_windows[m_windows.size() - 1] = window;
                break;
            }
        }

        setFocus(window);
        invalidate(window->bounds());
    }

    void WindowManager::sendToBack(Window *window)
    {
        if (!window)
            return;

        for (QC::usize i = 0; i < m_windows.size(); ++i)
        {
            if (m_windows[i] == window)
            {
                // Move to beginning (bottom of z-order)
                // Shift elements right to make room at front
                for (QC::usize j = i; j > 0; --j)
                {
                    m_windows[j] = m_windows[j - 1];
                }
                m_windows[0] = window;
                break;
            }
        }

        invalidate(window->bounds());
    }

    void WindowManager::invalidate(const Rect &rect)
    {
        if (m_compositor)
        {
            m_compositor->invalidate(rect);
        }
    }

    void WindowManager::render()
    {
        if (m_compositor)
        {
            m_compositor->compose();
        }
    }

    Size WindowManager::screenSize() const
    {
        if (m_framebuffer)
        {
            return Size{m_framebuffer->width(), m_framebuffer->height()};
        }
        return Size{0, 0};
    }

    Window *WindowManager::windowAt(Point p)
    {
        // Search from top to bottom (end to beginning)
        for (QC::isize i = static_cast<QC::isize>(m_windows.size()) - 1; i >= 0; --i)
        {
            Window *window = m_windows[static_cast<QC::usize>(i)];
            if (window->isVisible() && window->bounds().contains(p))
            {
                return window;
            }
        }
        return nullptr;
    }

    void WindowManager::routeMouseEvent(const QK::Event::MouseEventData &mouse)
    {
        using namespace QK::Event;

        m_mousePos = Point{mouse.x, mouse.y};

        Window *targetWindow = windowAt(m_mousePos);

        // Handle enter/leave
        if (targetWindow != m_hoveredWindow)
        {
            // TODO: Post MouseEnter/MouseLeave custom events if needed
            m_hoveredWindow = targetWindow;
        }

        // Route to target window
        if (targetWindow)
        {
            // For mouse down, focus the window
            if (mouse.type == Type::MouseButtonDown)
            {
                bringToFront(targetWindow);
            }

            // Translate coordinates into the window's local space so controls
            // can rely on window-relative positions for hit testing.
            QK::Event::MouseEventData localMouse = mouse;
            const Rect windowBounds = targetWindow->bounds();
            localMouse.x -= windowBounds.x;
            localMouse.y -= windowBounds.y;

            // Dispatch to window's onEvent
            QK::Event::Event event;
            event.data.mouse = localMouse;
            targetWindow->onEvent(event);
        }
    }

    void WindowManager::routeKeyEvent(const QK::Event::KeyEventData &key)
    {
        // Route keyboard events to focused window
        if (m_focusedWindow)
        {
            QK::Event::Event event;
            event.data.key = key;
            m_focusedWindow->onEvent(event);
        }
    }

    void WindowManager::postWindowEvent(QK::Event::Type type, Window *window)
    {
        auto &eventMgr = QK::Event::EventManager::instance();
        eventMgr.postWindowEvent(
            type,
            window->windowId(),
            window->bounds().x,
            window->bounds().y,
            window->bounds().width,
            window->bounds().height);
    }

    void WindowManager::applyStyleToWindow(Window *window, const StyleSnapshot &snapshot)
    {
        if (!window)
            return;
        window->setStyleSnapshot(&snapshot);
    }

    void WindowManager::onStyleChanged(const StyleSnapshot &snapshot)
    {
        for (QC::usize i = 0; i < m_windows.size(); ++i)
        {
            Window *window = m_windows[i];
            applyStyleToWindow(window, snapshot);
            if (window)
            {
                window->invalidate();
            }
        }
    }

} // namespace QW
