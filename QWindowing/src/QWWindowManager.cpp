// QWindowing Window Manager - Desktop window management implementation
// Namespace: QW

#include "QWWindowManager.h"
#include "QWWindow.h"
#include "QWFramebuffer.h"
#include "QWCompositor.h"
#include "QWStyleSystem.h"
#include "QWMessageBus.h"
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
          m_listenerId(QK::Event::InvalidListenerId),
          m_dragWindow(nullptr),
          m_dragOffset{0, 0},
          m_dragStartBounds{0, 0, 0, 0}
    {
    }

    WindowManager::~WindowManager()
    {
        shutdown();
    }

    void WindowManager::initialize(Framebuffer *fb)
    {
        m_framebuffer = fb;

        MessageBus::instance().initialize();

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

        // Capture bounds before removal so we can repaint the region that was covered.
        const Rect oldBounds = window->bounds();

        // If called from within a window's event handler, defer deletion until after
        // dispatch returns to avoid deleting 'this' while executing.
        if (m_dispatchDepth > 0)
        {
            // Avoid double-queue.
            for (QC::usize i = 0; i < m_pendingDestroy.size(); ++i)
            {
                if (m_pendingDestroy[i].window == window)
                {
                    return;
                }
            }

            // Post window destroy event immediately.
            postWindowEvent(QK::Event::Type::WindowDestroy, window);

            // Clear focus/hover references.
            if (m_focusedWindow == window)
                m_focusedWindow = nullptr;
            if (m_hoveredWindow == window)
                m_hoveredWindow = nullptr;

            // Remove from z-order list so it won't receive more events.
            for (QC::usize i = 0; i < m_windows.size(); ++i)
            {
                if (m_windows[i] == window)
                {
                    for (QC::usize j = i; j < m_windows.size() - 1; ++j)
                        m_windows[j] = m_windows[j + 1];
                    m_windows.pop_back();
                    break;
                }
            }

            window->setVisible(false);
            m_pendingDestroy.push_back(PendingDestroy{window, oldBounds});
            invalidate(oldBounds);
            return;
        }

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

        // Force the compositor to redraw what was underneath.
        invalidate(oldBounds);
    }

    void WindowManager::processPendingDestroy()
    {
        if (m_dispatchDepth != 0 || m_pendingDestroy.empty())
            return;

        for (QC::usize i = 0; i < m_pendingDestroy.size(); ++i)
        {
            delete m_pendingDestroy[i].window;
        }
        m_pendingDestroy.clear();
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

        // Keep hardware cursor position tightly synced to input events.
        // With a hardware cursor enabled, the compositor can update cursor registers
        // without needing to recomposite/present the whole frame.
        auto syncCursorNow = [&]()
        {
            // render() is safe here; compositor has an early-out for hw cursor + no dirty.
            render();
        };

        // If a title-bar drag is active, keep moving the captured window even if
        // the cursor leaves its bounds.
        if (m_dragWindow)
        {
            if (mouse.type == Type::MouseMove)
            {
                Rect oldBounds = m_dragWindow->bounds();
                QC::i32 newX = m_mousePos.x - m_dragOffset.x;
                QC::i32 newY = m_mousePos.y - m_dragOffset.y;

                // Clamp to screen bounds (minimal UX: keep window on-screen).
                const Size scr = screenSize();
                const QC::i32 maxX = (scr.width > oldBounds.width) ? (static_cast<QC::i32>(scr.width - oldBounds.width)) : 0;
                const QC::i32 maxY = (scr.height > oldBounds.height) ? (static_cast<QC::i32>(scr.height - oldBounds.height)) : 0;
                if (newX < 0)
                    newX = 0;
                if (newY < 0)
                    newY = 0;
                if (newX > maxX)
                    newX = maxX;
                if (newY > maxY)
                    newY = maxY;

                if (newX != oldBounds.x || newY != oldBounds.y)
                {
                    Rect newBounds = oldBounds;
                    newBounds.x = newX;
                    newBounds.y = newY;

                    invalidate(oldBounds);
                    m_dragWindow->setBounds(newBounds);
                    invalidate(newBounds);
                    postWindowEvent(QK::Event::Type::WindowMove, m_dragWindow);
                }

                syncCursorNow();
                return;
            }

            if (mouse.type == Type::MouseButtonUp && mouse.button == MouseButton::Left)
            {
                m_dragWindow = nullptr;
                syncCursorNow();
                return;
            }
        }

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

                // Title-bar drag for movable windows.
                // NOTE: The compositor chrome is currently minimal, so we treat the
                // top N pixels of the window as the title bar hit region.
                static constexpr QC::i32 kTitleBarHeight = 24;
                const Rect windowBounds = targetWindow->bounds();
                const bool isMovable = (targetWindow->flags() & WindowFlags::Movable) != 0;
                const bool hasTitle = (targetWindow->flags() & WindowFlags::HasTitle) != 0;
                const bool leftDown = (mouse.button == MouseButton::Left);
                const QC::i32 localY = m_mousePos.y - windowBounds.y;
                if (leftDown && isMovable && hasTitle && localY >= 0 && localY < kTitleBarHeight)
                {
                    // Give controls in the title area (e.g. terminal close button)
                    // first chance to handle the click.
                    QK::Event::MouseEventData localMouse = mouse;
                    localMouse.x -= windowBounds.x;
                    localMouse.y -= windowBounds.y;

                    QK::Event::Event downEvent;
                    downEvent.data.mouse = localMouse;
                    const bool handled = targetWindow->onEvent(downEvent);
                    if (handled)
                    {
                        syncCursorNow();
                        return;
                    }

                    // Not handled by controls; treat as a window move drag.
                    m_dragWindow = targetWindow;
                    m_dragOffset = Point{m_mousePos.x - windowBounds.x, m_mousePos.y - windowBounds.y};
                    m_dragStartBounds = windowBounds;
                    syncCursorNow();
                    return;
                }
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
            ++m_dispatchDepth;
            targetWindow->onEvent(event);
            --m_dispatchDepth;
            processPendingDestroy();
        }

        syncCursorNow();
    }

    void WindowManager::routeKeyEvent(const QK::Event::KeyEventData &key)
    {
        // Route keyboard events to focused window
        if (m_focusedWindow)
        {
            QK::Event::Event event;
            event.data.key = key;
            ++m_dispatchDepth;
            m_focusedWindow->onEvent(event);
            --m_dispatchDepth;
            processPendingDestroy();
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
